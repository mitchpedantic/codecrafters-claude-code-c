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
extern int claude_API_init(void);
extern int claude_API_call(char *call_call_body, cJSON** api_reply);
extern void claude_API_fin(void);

#endif //CLAUDE_API_H__