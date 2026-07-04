/************************************
 * INCLUDES
 ************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "claude_api.h"

/************************************
 * PRIVATE MACROS AND DEFINES
 ************************************/
#define SUCCESS (0U)
#define NULL_ERROR (1U)
#define NO_CHOICE (2U)
#define FILE_ERROR (3U)
#define STUB_ERROR (4U)
#define JOB_DONE (5U)

/************************************
 * STATIC VARIABLES
 ************************************/
static cJSON *conversation = NULL;

/************************************
 * STATIC FUNCTION PROTOTYPES
 ************************************/
static long wrap_ftell(FILE *fd);
static int read_function(char* source, char **tool_result);
static cJSON* get_tools(void);
static int wrap_API_call(cJSON** api_reply);
static int start_conversation(const char *prompt);
static int handle_tool_call(cJSON *tool_call);
static int handle_choice(cJSON *choice);
static int loop(void);
/************************************
 * STATIC FUNCTIONS
 ************************************/
static long
wrap_ftell(FILE *fd) {
    long fsize = 0;
    if (fseek(fd, 0, SEEK_END) != 0) {
        return -1;
    }
    if (fsize = ftell(fd), fsize < 0) {
        return -1;
    }
    rewind(fd);
    return fsize;
}

static int
read_function(char* source, char **tool_result) {
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
        FILE *fd = fopen(file,"r");

        size_t fsize = wrap_ftell(fd);
        if (fsize < 0) {
            fprintf(stderr, "Failed to get file size\n");
            fclose(fd);
            return FILE_ERROR;
        }
        
        *tool_result = malloc((size_t)fsize + 1);
        if (!tool_result) {
            fprintf(stderr, "malloc failed\n");
            fclose(fd);
            return NULL_ERROR;
        }
        
        size_t read = 0;
        if (read = fread(*tool_result, 1, (size_t)fsize, fd), !read) {
            fprintf(stderr, "empty\n");
            fclose(fd);
            return FILE_ERROR;
        }
        //char c = 0;
        //while (c = fgetc(fp), EOF != c) {
            //    putc(c, stdout);
        //}
        fclose(fd);
    }
    
    return SUCCESS;
}

static cJSON*
get_tools(void) {
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

    return tool;
}

static int
wrap_API_call(cJSON** api_reply) {
    int returncode = SUCCESS;
    cJSON *api_request = cJSON_CreateObject();
    cJSON_AddStringToObject(api_request, "model", "anthropic/claude-haiku-4.5");

    // tools
    cJSON *tools = cJSON_AddArrayToObject(api_request, "tools");
    cJSON *tool = NULL;
    if (!tools || (tool = get_tools(), !tool)) {
        fprintf(stderr, "Failed to set tools.\n");
        cJSON_Delete(api_request);
        return NULL_ERROR;
    }
    cJSON_AddItemToArray(tools, tool);

    // messages
    cJSON *tmp_copy = NULL;
    if (tmp_copy = cJSON_Duplicate(conversation, 1), !tmp_copy) {
        fprintf(stderr, "Failed to copy conversation.\n");
        cJSON_Delete(api_request);
        return NULL_ERROR;
    }
    cJSON_AddItemToObject(api_request, "messages", tmp_copy);

    char *body = cJSON_PrintUnformatted(api_request);
    cJSON_Delete(api_request);
    returncode = claude_API_call(body, api_reply);
    free(body);

    return returncode;
}

static int
start_conversation(const char *prompt) {
    cJSON *user_message = cJSON_CreateObject();
    cJSON_AddStringToObject(user_message, "role", "user");
    cJSON_AddStringToObject(user_message, "content", prompt);
    cJSON_AddItemToArray(conversation, user_message);
}

static int
handle_tool_call(cJSON *tool_call) {
    // fprintf(stderr, cJSON_Print(tool_call));
    cJSON *id = cJSON_GetObjectItem(tool_call, "id");
    if (!cJSON_IsString(id)) {
        fprintf(stderr, "\"id\" is not a string.\n");
        return STUB_ERROR;
    }
    //
    cJSON *function = cJSON_GetObjectItem(tool_call, "function");
    //
    cJSON *name = cJSON_GetObjectItem(function, "name");
    //
    char *tool_result = NULL;
    if (cJSON_IsString(name) && (name->valuestring != NULL)) {
        if (0 != strcmp(name->valuestring, "Read")) {
            return SUCCESS;
        }
        cJSON *name = cJSON_GetObjectItem(function, "name");
        if (!cJSON_IsString(name) || (name->valuestring == NULL)) {
            fprintf(stderr, "\"name\" is not a string.\n");
            return SUCCESS;
        }
        
        cJSON *arguments = cJSON_GetObjectItem(function, "arguments");
        
        if (!cJSON_IsString(arguments) || (arguments->valuestring == NULL)) {
            fprintf(stderr, "\"arguments\" is not a string.\n");
            return SUCCESS;
        }

        int returncode = read_function(arguments->valuestring, &tool_result);
        if (returncode) {
            fprintf(stderr, "\"Read\" tool failed.\n");
            if (returncode != NULL_ERROR) free(tool_result);
            return returncode;
        }
        cJSON *call_report = cJSON_CreateObject();
        cJSON_AddStringToObject(call_report, "role", "tool");
        cJSON_AddStringToObject(call_report, "tool_call_id", id->valuestring);
        cJSON_AddStringToObject(call_report, "content", tool_result);
        cJSON_AddItemToArray(conversation, call_report);
        free(tool_result);
    }

    return SUCCESS;
}

static int
handle_choice(cJSON *choice) {
    int returncode = SUCCESS;
    cJSON *message = cJSON_GetObjectItem(choice, "message");
    // add to conversation
    {
        cJSON* tmp = cJSON_Duplicate(message, 1);
        if (!tmp) return NULL_ERROR;
        cJSON_AddItemToArray(conversation, tmp);
    }
    //
    int n_calls = 0;
    cJSON *content = cJSON_GetObjectItem(message, "content");
    cJSON *tool_calls = cJSON_GetObjectItem(message, "tool_calls");
    if (cJSON_IsArray(tool_calls))
        n_calls = cJSON_GetArraySize(tool_calls);
    // TASK 1
    if ((cJSON_IsString(content)) && !n_calls) {
        printf("%s", content->valuestring);
        return JOB_DONE;
    }
    // TASK 2
    if (n_calls == 0)
        return returncode;

    for (int i = 0; (i < n_calls) && (!returncode); i++) {
        cJSON *call = cJSON_GetArrayItem(tool_calls, i);
        returncode = handle_tool_call(call);
    }

    return returncode;
}

static int
loop(void) {
    fprintf(stderr, cJSON_Print(conversation));
    int returncode = SUCCESS;
    cJSON *api_reply = NULL;
    if (returncode = wrap_API_call(&api_reply)) {
        if (api_reply) cJSON_Delete(api_reply);
        fprintf(stderr, "Failed to parse response JSON\n");
        return returncode;
    }
    
    int n_choice = 0;
    cJSON *choices = cJSON_GetObjectItem(api_reply, "choices");
    if (!cJSON_IsArray(choices) || (n_choice = cJSON_GetArraySize(choices), n_choice == 0)) {
        fprintf(stderr, "No choices in response\n");
        cJSON_Delete(api_reply);
        return NO_CHOICE;
    }
    
    for (int i = 0; (i < 1) && (!returncode); i++) {
        cJSON *choice = cJSON_GetArrayItem(choices, i);
        returncode = handle_choice(choice);
    }

    cJSON_Delete(api_reply);
    return returncode;
}

/************************************
 * GLOBAL FUNCTIONS
 ************************************/
int
main(int argc, char *argv[]) {
    int returncode = SUCCESS;
    const char *prompt = NULL;
    if (getopt(argc, argv, "p:") == 'p') prompt = optarg;
    if (!prompt) {
        fprintf(stderr, "error: -p flag is required\n");
        return 1;
    }

    conversation = cJSON_CreateArray();
    start_conversation(prompt);

    // API initialization, claude_api handles the interface with curl
    claude_API_init();
    
    // create the context
    while (returncode = loop(), !returncode) {}

    claude_API_fin();
    cJSON_Delete(conversation);
    return SUCCESS;
}
