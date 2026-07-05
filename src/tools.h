#ifndef TOOLS_H__
#define TOOLS_H__

/************************************
 * INCLUDES
 ************************************/
#include <cjson/cJSON.h>

/************************************
 * MACROS AND DEFINES
 ************************************/

/************************************
 * TYPEDEFS
 ************************************/

/************************************
 * EXPORTED VARIABLES
 ************************************/

/************************************
 * GLOBAL FUNCTION PROTOTYPES
 ************************************/
/*!
 * @brief Creates the tools' array (JSON) with the tools available for claude
 */
extern cJSON* init_tools(void);
/*!
 * @brief Handler for the request of use for a tool.
 * @param[in] request, the request issued by claude to use a certain tool
 * @returns cJSON*, a pointer to the result of the tool call. Returns NULL upon failure
 */
extern cJSON* use_tool(cJSON* request);

#endif //TOOLS_H__
