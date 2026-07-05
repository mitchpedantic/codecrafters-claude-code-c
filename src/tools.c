/************************************
 * INCLUDES
 ************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tools.h"

/************************************
 * PRIVATE MACROS AND DEFINES
 ************************************/

/************************************
 * PRIVATE TYPEDEFS
 ************************************/
typedef enum {
    tool_success,
    file_error,
    nullmem_error, // a stdlib function returned null, free shall not be called
    invalid_error, // a parsing error, a field that was expected to be there is actually absent
} internal_codes_e;

/************************************
 * STATIC VARIABLES
 ************************************/

/************************************
 * STATIC FUNCTION PROTOTYPES
 ************************************/
/*!
 * @brief utility function that handles the creation of the Read tool
 */
static cJSON* create_read(void);
/*!
 * @brief utility function that handles the creation of the Write tool
 */
static cJSON* create_write(void);
/*!
 * @brief utility function that handles the creation of the Bash tool
 */
static cJSON* create_bash(void);
/*!
 * @brief utility function that wraps the ftell function to handle error graciously
 */
static long wrap_ftell(FILE* fd);
/*!
 * @brief utility function that copies the content in a newly allocated memory space
 */
static internal_codes_e copy_file(FILE* fd, long fsize, char** tool_result);
/*!
 * @brief utility function that handles calls to read tool
 */
static internal_codes_e handle_read(cJSON* arguments, char** tool_result);
/*!
 * @brief utility function that handles calls to write tool
 */
static internal_codes_e handle_write(cJSON* arguments, char** tool_result);
/*!
 * @brief utility function that handles calls to bash tool
 */
static internal_codes_e handle_bash(cJSON* arguments, char** tool_result);
/*!
 * @brief utility function that handles the selection of tools
 */
static internal_codes_e select_tool(cJSON* function, cJSON* report);
/************************************
 * STATIC FUNCTIONS
 ************************************/
static cJSON*
create_read(void) {
    cJSON* tool = cJSON_CreateObject();
    cJSON_AddStringToObject(tool, "type", "function");
    {
        cJSON* function = cJSON_CreateObject();
        cJSON_AddStringToObject(function, "name", "Read");
        cJSON_AddStringToObject(function, "description", "Read and return the contents of a file");
        {
            cJSON* params = cJSON_CreateObject();
            cJSON_AddStringToObject(params, "type", "object");
            {
                cJSON* requirements = cJSON_AddArrayToObject(params, "required");
                cJSON_AddItemToArray(requirements, cJSON_CreateString("file_path"));
            }
            {
                cJSON* properties = cJSON_CreateObject();
                {
                    cJSON* filepath = cJSON_CreateObject();
                    cJSON_AddStringToObject(filepath, "type", "string");
                    cJSON_AddStringToObject(filepath, "description", "The path to the file to read");
                    cJSON_AddItemToObject(properties, "file_path", filepath);
                }
                cJSON_AddItemToObject(params, "properties", properties);
            }
            cJSON_AddItemToObject(function, "parameters", params);
        }
        cJSON_AddItemToObject(tool, "function", function);
    }

    return tool;
}

static cJSON*
create_write(void) {
    cJSON* tool = cJSON_CreateObject();
    cJSON_AddStringToObject(tool, "type", "function");
    {
        cJSON* function = cJSON_CreateObject();
        cJSON_AddStringToObject(function, "name", "Write");
        cJSON_AddStringToObject(function, "description", "Write content to a file");
        {
            cJSON* params = cJSON_CreateObject();
            cJSON_AddStringToObject(params, "type", "object");
            {
                cJSON* requirements = cJSON_AddArrayToObject(params, "required");
                cJSON_AddItemToArray(requirements, cJSON_CreateString("file_path"));
                cJSON_AddItemToArray(requirements, cJSON_CreateString("content"));
            }
            {
                cJSON* properties = cJSON_CreateObject();
                {
                    cJSON* filepath = cJSON_CreateObject();
                    cJSON_AddStringToObject(filepath, "type", "string");
                    cJSON_AddStringToObject(filepath, "description", "The path of the file to write to");
                    cJSON_AddItemToObject(properties, "file_path", filepath);
                }
                {
                    cJSON* filepath = cJSON_CreateObject();
                    cJSON_AddStringToObject(filepath, "type", "string");
                    cJSON_AddStringToObject(filepath, "description", "The content to write to the file");
                    cJSON_AddItemToObject(properties, "content", filepath);
                }
                cJSON_AddItemToObject(params, "properties", properties);
            }
            cJSON_AddItemToObject(function, "parameters", params);
        }
        cJSON_AddItemToObject(tool, "function", function);
    }
    return tool;
}

static cJSON*
create_bash(void) {
    cJSON* tool = cJSON_CreateObject();
    cJSON_AddStringToObject(tool, "type", "function");
    {
        cJSON* function = cJSON_CreateObject();
        cJSON_AddStringToObject(function, "name", "Bash");
        cJSON_AddStringToObject(function, "description", "Execute a shell command");
        {
            cJSON* params = cJSON_CreateObject();
            cJSON_AddStringToObject(params, "type", "object");
            {
                cJSON* requirements = cJSON_AddArrayToObject(params, "required");
                cJSON_AddItemToArray(requirements, cJSON_CreateString("command"));
            }
            {
                cJSON* properties = cJSON_CreateObject();
                {
                    cJSON* filepath = cJSON_CreateObject();
                    cJSON_AddStringToObject(filepath, "type", "string");
                    cJSON_AddStringToObject(filepath, "description", "The command to execute");
                    cJSON_AddItemToObject(properties, "command", filepath);
                }
                cJSON_AddItemToObject(params, "properties", properties);
            }
            cJSON_AddItemToObject(function, "parameters", params);
        }
        cJSON_AddItemToObject(tool, "function", function);
    }
    return tool;
}

static long
wrap_ftell(FILE* fd) {
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

static internal_codes_e
copy_file(FILE* fd, long fsize, char** tool_result) {
    *tool_result = malloc((size_t)fsize + 1);
    if (!tool_result) {
        fprintf(stderr, "malloc failed\n");
        fclose(fd);
        return nullmem_error;
    }
    
    size_t read = 0;
    if (read = fread(*tool_result, 1, (size_t)fsize, fd), !read) {
        fprintf(stderr, "empty\n");
        fclose(fd);
        return file_error;
    }
    (*tool_result)[read] = '\0';

    return tool_success;
}

static internal_codes_e
handle_read(cJSON* arguments, char** tool_result) {
    internal_codes_e returncode = tool_success;
    
    cJSON* file_path = cJSON_GetObjectItem(arguments, "file_path");
    if (!cJSON_IsString(file_path) || (!file_path->valuestring)) {
        fprintf(stderr, "\"file_path\" is not a string\n");
        return invalid_error;
    }
        
    FILE* fd = fopen(file_path->valuestring,"r");
    size_t fsize = wrap_ftell(fd);
    if (fsize < 0) {
        fprintf(stderr, "Failed to get file size\n");
        fclose(fd);
        return file_error;
    }
    
    returncode = copy_file(fd, fsize, tool_result);
    fclose(fd);
    
    return returncode;
}

static internal_codes_e
handle_write(cJSON* arguments, char** tool_result) {
    internal_codes_e returncode = tool_success;
    
    cJSON* file_path = cJSON_GetObjectItem(arguments, "file_path");
    if (!cJSON_IsString(file_path) || (!file_path->valuestring)) {
        fprintf(stderr, "\"file_path\" is not a string\n");
        return invalid_error;
    }
    
    cJSON* content = cJSON_GetObjectItem(arguments, "content");
    if (!cJSON_IsString(content) || (!content->valuestring)) {
        fprintf(stderr, "\"content\" is not a string\n");
        return invalid_error;
    }

    FILE* fd = fopen(file_path->valuestring,"w+");
    for (int i = 0; content->valuestring[i] != '\0' ; i++) {
        putc(content->valuestring[i], fd);
    }
    fclose(fd);

    return returncode;
}

static internal_codes_e
handle_bash(cJSON* arguments, char** tool_result) {
    char output[1035];
    size_t current_size = 0;
    size_t allocation = 1035;

    cJSON* command = cJSON_GetObjectItem(arguments, "command");
    if (!cJSON_IsString(command) || (!command->valuestring)) {
        fprintf(stderr, "\"command\" is not a string\n");
        return invalid_error;
    }
    
    *tool_result = malloc(allocation);
    if (!tool_result) {
        fprintf(stderr, "malloc failed\n");
        return nullmem_error;
    }
    
    FILE *process_pipe = popen(command->valuestring, "r");
    if (!process_pipe) {
      fprintf(stderr, "%s : process cannot be created.\n", command->valuestring);
      return file_error;
    }
    
    while (fgets(output, sizeof(output), process_pipe) != NULL) {
        size_t addlen = strlen(output);
        memcpy((*tool_result) + current_size, output, addlen);
        current_size += addlen;
        if ((current_size * 2) >= allocation) {
            char *tmp = NULL;
            allocation *= 2;
            *tool_result = realloc(tmp = *tool_result, allocation);
            if (!*tool_result) {
                free(tmp);
                return nullmem_error;
            }
        }
    }
    (*tool_result)[current_size] = '\0';

    pclose(process_pipe);
}

static internal_codes_e
select_tool(cJSON* function, cJSON* report) {
    internal_codes_e returncode = tool_success;
    internal_codes_e (*handler)(cJSON *, char **);

    cJSON* name = cJSON_GetObjectItem(function, "name");
    if (!cJSON_IsString(name) || (!name->valuestring)) {
        fprintf(stderr, "\"name\" is not a string\n");
        return invalid_error;
    }
        
    if (0 == strcmp(name->valuestring, "Read")) {
        handler = handle_read;
    }
    else if (0 == strcmp(name->valuestring, "Write")) {
        handler = handle_write;
    }
    else if (0 == strcmp(name->valuestring, "Bash")) {
        handler = handle_bash;
    }
    else {
        fprintf(stderr, "%s is not a valid tool\n", name->valuestring);
        return invalid_error;
    }
    
    char* tool_result = NULL;
    cJSON* arguments = NULL;
    {
        cJSON* tmp = cJSON_GetObjectItem(function, "arguments");
        if (!cJSON_IsString(tmp) || (!tmp->valuestring)) {
            fprintf(stderr, "\"arguments\" is incomplete\n");
            return invalid_error;
        }
        arguments = cJSON_Parse(tmp->valuestring);
    }
    returncode = handler(arguments, &tool_result);
    if (returncode) {
        fprintf(stderr, "%s tool failed.\n", name->valuestring);
        if ((returncode != nullmem_error) && tool_result) free(tool_result);
        return returncode;
    }
    
    cJSON_AddStringToObject(report, "content", tool_result);
    if (tool_result)
        free(tool_result);

    return tool_success;
}

/************************************
 * GLOBAL FUNCTIONS
 ************************************/
extern cJSON*
init_tools(void) {
    cJSON* tools = cJSON_CreateArray();
    cJSON_AddItemToArray(tools, create_read());
    cJSON_AddItemToArray(tools, create_write());
    cJSON_AddItemToArray(tools, create_bash());
    return tools;
}

extern cJSON*
use_tool(cJSON* request) {
    internal_codes_e returncode = tool_success;
    cJSON* id = cJSON_GetObjectItem(request, "id");
    if (!cJSON_IsString(id)) {
        fprintf(stderr, "\"id\" is not a string.\n");
        return NULL;
    }

    cJSON* function = cJSON_GetObjectItem(request, "function");
    if (!function) {
        fprintf(stderr, "\"function\" is null.\n");
        return NULL;
    }
    
    cJSON* call_report = cJSON_CreateObject();
    cJSON_AddStringToObject(call_report, "role", "tool");
    cJSON_AddStringToObject(call_report, "tool_call_id", id->valuestring);

    returncode = select_tool(function, call_report);
    if (returncode) {
        fprintf(stderr, "The tool cannot be executed.\n");
        cJSON_Delete(call_report);
        return NULL;
    }

    return call_report;
}