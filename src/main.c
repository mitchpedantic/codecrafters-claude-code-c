/************************************
 * INCLUDES
 ************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "tools.h"
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
static cJSON* conversation = NULL;

/************************************
 * STATIC FUNCTION PROTOTYPES
 ************************************/
static int wrap_API_call(cJSON** api_reply);
static int start_conversation(const char* prompt);
static int handle_tool_call(cJSON* tool_call);
static int handle_choice(cJSON* choice);
static int loop(void);

/************************************
 * STATIC FUNCTIONS
 ************************************/
static int
wrap_API_call(cJSON** api_reply) {
    int returncode = SUCCESS;
    cJSON* api_request = cJSON_CreateObject();
    cJSON_AddStringToObject(api_request, "model", "anthropic/claude-haiku-4.5");

    // tools
    cJSON* tools = init_tools();
    if (!tools) {
        fprintf(stderr, "Failed to set tools.\n");
        cJSON_Delete(api_request);
        return NULL_ERROR;
    }
    cJSON_AddItemToObject(api_request, "tools", tools);

    // messages
    cJSON* tmp_copy = NULL;
    if (tmp_copy = cJSON_Duplicate(conversation, 1), !tmp_copy) {
        fprintf(stderr, "Failed to copy conversation.\n");
        cJSON_Delete(api_request);
        return NULL_ERROR;
    }
    cJSON_AddItemToObject(api_request, "messages", tmp_copy);

    char* body = cJSON_PrintUnformatted(api_request);
    cJSON_Delete(api_request);
    returncode = claude_API_call(body, api_reply);
    free(body);

    return returncode;
}

static int
start_conversation(const char* prompt) {
    cJSON* user_message = cJSON_CreateObject();
    cJSON_AddStringToObject(user_message, "role", "user");
    cJSON_AddStringToObject(user_message, "content", prompt);
    cJSON_AddItemToArray(conversation, user_message);
}

static int
handle_tool_call(cJSON* tool_call) {
    // fprintf(stderr, cJSON_Print(tool_call));
    cJSON* call_report = use_tool(tool_call);
    if (!call_report) {
        fprintf(stderr, "Failed to use a tool\n");
        return NULL_ERROR;
    }

    cJSON_AddItemToArray(conversation, call_report);
    return SUCCESS;
}

static int
handle_choice(cJSON* choice) {
    int returncode = SUCCESS;
    cJSON* message = cJSON_GetObjectItem(choice, "message");
    // add to conversation
    {
        cJSON* tmp = cJSON_Duplicate(message, 1);
        if (!tmp) return NULL_ERROR;
        cJSON_AddItemToArray(conversation, tmp);
    }
    //
    int n_calls = 0;
    cJSON* content = cJSON_GetObjectItem(message, "content");
    cJSON* tool_calls = cJSON_GetObjectItem(message, "tool_calls");
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
        cJSON* call = cJSON_GetArrayItem(tool_calls, i);
        returncode = handle_tool_call(call);
    }

    return returncode;
}

static int
loop(void) {
    //fprintf(stderr, cJSON_Print(conversation));
    int returncode = SUCCESS;
    cJSON* api_reply = NULL;
    if (returncode = wrap_API_call(&api_reply)) {
        if (api_reply) cJSON_Delete(api_reply);
        fprintf(stderr, "Failed to parse response JSON\n");
        return returncode;
    }
    
    int n_choice = 0;
    cJSON* choices = cJSON_GetObjectItem(api_reply, "choices");
    if (!cJSON_IsArray(choices) || (n_choice = cJSON_GetArraySize(choices), n_choice == 0)) {
        fprintf(stderr, "No choices in response\n");
        cJSON_Delete(api_reply);
        return NO_CHOICE;
    }
    
    for (int i = 0; (i < 1) && (!returncode); i++) {
        cJSON* choice = cJSON_GetArrayItem(choices, i);
        returncode = handle_choice(choice);
    }

    cJSON_Delete(api_reply);
    return returncode;
}

/************************************
 * GLOBAL FUNCTIONS
 ************************************/
int
main(int argc, char* argv[]) {
    int returncode = SUCCESS;
    const char* prompt = NULL;
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
