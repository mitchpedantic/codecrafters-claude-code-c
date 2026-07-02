#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>

struct response_buf {
    char *data;
    size_t size;
};

static size_t curl_write_response(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t total = size * nmemb;
    struct response_buf *buf = (struct response_buf *)userp;
    char *tmp = realloc(buf->data, buf->size + total + 1);
    if (!tmp) return 0;
    buf->data = tmp;
    memcpy(buf->data + buf->size, contents, total);
    buf->size += total;
    buf->data[buf->size] = '\0';
    return total;
}

static int read_function(char* source) {
    static char file[4096U];
    (void *)memset(file, '\0', 4096U);
    char *token = strtok(source, "\"");
    // skip first match (start of the word! which is '{')
    while (token = strtok(NULL, "\""), token) {
        if (0 == strncmp(token, "file_path", sizeof("file_path"))) {
            token = strtok(NULL, ": \"");
            for (int i = 0U; token[i] != '\"'; i++)
                file[i] = token[i];
            break;
        }
        token = strtok(NULL, "\"");
    }
    if (file) {
        FILE *fp = fopen(file,"r");
        char c = 0;
        while (c = fgetc(fp), EOF != c) {
            putc(c, stdout);
        }
        fclose(fp);
    }

    return 0;
}

int main(int argc, char *argv[]) {
    const char *prompt = NULL;
    if (getopt(argc, argv, "p:") == 'p') prompt = optarg;
    if (!prompt) {
        fprintf(stderr, "error: -p flag is required\n");
        return 1;
    }

    const char *api_key = getenv("OPENROUTER_API_KEY");
    const char *base_url = getenv("OPENROUTER_BASE_URL");
    if (!base_url || !*base_url) base_url = "https://openrouter.ai/api/v1";
    if (!api_key || !*api_key) {
        fprintf(stderr, "OPENROUTER_API_KEY is not set\n");
        return 1;
    }

    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "model", "anthropic/claude-haiku-4.5");
    
    // tools
    cJSON *tools = cJSON_AddArrayToObject(req, "tools");
    cJSON *tool = cJSON_CreateObject();
    cJSON_AddStringToObject(tool, "type", "function");
    cJSON *function = cJSON_CreateObject();
    cJSON_AddStringToObject(function, "name", "Read");
    cJSON_AddStringToObject(function, "description", "Read and return the contents of a file");
    cJSON *params = cJSON_CreateObject();
    cJSON_AddStringToObject(params, "type", "object");
    cJSON *properties = cJSON_CreateObject();
    cJSON *filepath = cJSON_CreateObject();
    cJSON_AddStringToObject(filepath, "type", "string");
    cJSON_AddStringToObject(filepath, "description", "The path to the file to read");
    cJSON_AddItemToObject(properties, "file_path", filepath);
    cJSON *requirements = cJSON_AddArrayToObject(params, "required");
    cJSON_AddItemToArray(requirements, cJSON_CreateString("file_path"));
    cJSON_AddItemToObject(params, "properties", properties);
    cJSON_AddItemToObject(function, "parameters", params);
    cJSON_AddItemToObject(tool, "function", function);
    cJSON_AddItemToArray(tools, tool);

    // messages
    cJSON *messages = cJSON_AddArrayToObject(req, "messages");
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "role", "user");
    cJSON_AddStringToObject(msg, "content", prompt);
    cJSON_AddItemToArray(messages, msg);

    char *body = cJSON_PrintUnformatted(req);
    cJSON_Delete(req);

    char url[512];
    snprintf(url, sizeof(url), "%s/chat/completions", base_url);

    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", api_key);

    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL *curl = curl_easy_init();
    struct response_buf resp = {NULL, 0};
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, auth_header);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_response);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);

    CURLcode res = curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    free(body);

    if (res != CURLE_OK) {
        fprintf(stderr, "curl error: %s\n", curl_easy_strerror(res));
        free(resp.data);
        return 1;
    }

    cJSON *json = cJSON_Parse(resp.data);
    free(resp.data);
    if (!json) {
        fprintf(stderr, "Failed to parse response JSON\n");
        return 1;
    }

    int n_choice = 0;
    cJSON *choices = cJSON_GetObjectItem(json, "choices");
    if (!cJSON_IsArray(choices) || (n_choice = cJSON_GetArraySize(choices), n_choice == 0)) {
        fprintf(stderr, "no choices in response\n");
        cJSON_Delete(json);
        return 1;
    }
    
    for (int i = 0; i < n_choice; i++) {
        cJSON *first = cJSON_GetArrayItem(choices, i);
        cJSON *message = cJSON_GetObjectItem(first, "message");
        cJSON *content = cJSON_GetObjectItem(message, "content");
        cJSON *tool_calls = cJSON_GetObjectItem(message, "tool_calls");
        //
        // You can use print statements as follows for debugging, they'll be visible when running tests.
        // fprintf(stderr, "Logs from your program will appear here!\n");
        //
        // pass test task 1
        if (cJSON_GetStringValue(content))
            // TODO: Uncomment the line below to pass the first stage
            printf("%s", cJSON_GetStringValue(content));
        // pass test task 2
        int n_calls = 0;
        if (!cJSON_IsArray(tool_calls) || (n_calls = cJSON_GetArraySize(tool_calls), n_calls == 0))
            continue;
        for (int j = 0; j < n_calls; j++) {
            cJSON *call = cJSON_GetArrayItem(tool_calls, j);
            cJSON *function = cJSON_GetObjectItem(call, "function");
            //
            cJSON *name = cJSON_GetObjectItem(function, "name");
            if (cJSON_IsString(name) && (name->valuestring != NULL)) {
                if (0 == strcmp(name->valuestring, "Read")) {
                    cJSON *name = cJSON_GetObjectItem(function, "name");
                    if (cJSON_IsString(name) && (name->valuestring != NULL)) {
                        cJSON *arguments = cJSON_GetObjectItem(function, "arguments");
                        cJSON_IsString(arguments) && \
                        (arguments->valuestring != NULL) && \
                        read_function(arguments->valuestring);
                    }
                }
            }
            
        }
    }

    cJSON_Delete(json);
    return 0;
}
