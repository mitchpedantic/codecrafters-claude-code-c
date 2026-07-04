/************************************
 * INCLUDES
 ************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#include "claude_api.h"

/************************************
 * PRIVATE MACROS AND DEFINES
 ************************************/

/************************************
 * PRIVATE TYPEDEFS
 ************************************/
struct response_buf {
    char *data;
    size_t size;
};

/************************************
 * STATIC VARIABLES
 ************************************/
static char url[512] = {'\0'};
static char auth_header[512] = {'\0'};
static const char *api_key = NULL;
static const char *base_url = NULL;

/************************************
 * STATIC FUNCTION PROTOTYPES
 ************************************/
static size_t curl_write_response(void *contents, size_t size, size_t nmemb, void *userp);

/************************************
 * STATIC FUNCTIONS
 ************************************/
static size_t
curl_write_response(void *contents, size_t size, size_t nmemb, void *userp) {
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

/************************************
 * GLOBAL FUNCTIONS
 ************************************/
extern int
claude_API_init(void) {
    api_key = getenv("OPENROUTER_API_KEY");
    base_url = getenv("OPENROUTER_BASE_URL");
    if (!base_url || !*base_url) base_url = "https://openrouter.ai/api/v1";
    if (!api_key || !*api_key) {
        fprintf(stderr, "OPENROUTER_API_KEY is not set\n");
        return CLAUDE_API_FAILURE;
    }

    snprintf(url, sizeof(url), "%s/chat/completions", base_url);
    
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", api_key);

    curl_global_init(CURL_GLOBAL_DEFAULT);

    return CLAUDE_API_SUCCESS;
}

extern void
claude_API_fin(void) {
    curl_global_cleanup();
}

extern int
claude_API_call(char *call_body, cJSON** api_reply) {
    if (!api_key || !base_url) {
        fprintf(stderr, "OPENROUTER_API_KEY is not set\n");
        return CLAUDE_API_FAILURE;
    }

    CURL *curl = curl_easy_init();
    struct response_buf resp = {NULL, 0};
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, auth_header);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, call_body);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_response);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);

    CURLcode res = curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    if (res != CURLE_OK) {
        fprintf(stderr, "curl error: %s\n", curl_easy_strerror(res));
        free(resp.data);
        return CLAUDE_API_FAILURE;
    }
    
    *api_reply = cJSON_Parse(resp.data);
    free(resp.data);

    return CLAUDE_API_SUCCESS;
}
