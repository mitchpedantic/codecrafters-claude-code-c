#ifndef CLAUDE_API_H__
#define CLAUDE_API_H__

/************************************
 * INCLUDES
 ************************************/
#include <cjson/cJSON.h>

/************************************
 * MACROS AND DEFINES
 ************************************/
#define CLAUDE_API_SUCCESS (0U)
#define CLAUDE_API_FAILURE (1U)

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
 * @brief initialize the communication with claude's API
 */
extern int claude_API_init(void);
/*!
 * @brief handler for calls to claude's API
 * @param[in] call_body, pointer to the request string
 * @param[in,out] api_reply, pointer to the address of the API reply (initialized to NULL before the call)
 */
extern int claude_API_call(char *call_body, cJSON** api_reply);
/*!
 * @brief dispose of any resource acquired by curl
 */
extern void claude_API_fin(void);

#endif //CLAUDE_API_H__