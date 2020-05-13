/**
 * @file
 * Contains a number of types commonly used by the ZAF.
 * @copyright 2018 Silicon Laboratories Inc.
 */

#ifndef ZAF_APPLICATIONUTILITIES_ZAF_TYPES_H_
#define ZAF_APPLICATIONUTILITIES_ZAF_TYPES_H_

#include <ZW_security_api.h>
#include <ZW_classcmd.h>

typedef struct _MULTICHAN_SOURCE_NODE_ID_
{
  uint8_t nodeId;           /* uint8_t 0 */
  uint8_t endpoint   : 7;   /* uint8_t 1, bit 0-6 */
  uint8_t res : 1;          /* uint8_t 1, bit 7 */
} MULTICHAN_SOURCE_NODE_ID;

typedef struct _MULTICHAN_DEST_NODE_ID_
{
  uint8_t nodeId;           /* uint8_t 0 */
  uint8_t endpoint   : 7;   /* uint8_t 1, bit 0-6 */
  uint8_t BitAddress : 1;   /* uint8_t 1, bit 7 */
} MULTICHAN_DEST_NODE_ID;

typedef struct _RECEIVE_OPTIONS_TYPE_EX_ {
  uint8_t  rxStatus;           /* Frame header info */
  security_key_t securityKey;
  MULTICHAN_SOURCE_NODE_ID sourceNode;
  MULTICHAN_DEST_NODE_ID destNode;
} RECEIVE_OPTIONS_TYPE_EX;


/**
 * @enum e_cmd_handler_return_code_t
 */
typedef enum
{
  E_CMD_HANDLER_RETURN_CODE_FAIL,           ///< the command was not accepted or accepted but failed to execute. Command class returns FAIL
  E_CMD_HANDLER_RETURN_CODE_HANDLED,        ///< the command was accepted and executed by the command handler. Command class returns SUCCESS
  E_CMD_HANDLER_RETURN_CODE_WORKING,        ///< the command was accepted but is not fully executed. Command class returns WORKING
  E_CMD_HANDLER_RETURN_CODE_NOT_SUPPORTED,  ///< the command handler does not support this command. Command class returns NO_SUPPORT
} e_cmd_handler_return_code_t;

/**
 * Status on incoming frame. Use same values as cc_supervision_status_t
 */
typedef enum
{
  RECEIVED_FRAME_STATUS_NO_SUPPORT = 0x00, /**< Frame not supported*/
  RECEIVED_FRAME_STATUS_FAIL = 0x02,       /**< Could not handle incoming frame*/
  RECEIVED_FRAME_STATUS_SUCCESS = 0xFF     /**< Frame received successfully*/
} received_frame_status_t;

typedef struct
{
  uint16_t CC;
  uint16_t version;
  received_frame_status_t(*pHandler)(RECEIVE_OPTIONS_TYPE_EX *, ZW_APPLICATION_TX_BUFFER *, uint8_t);
}
CC_handler_map_t;


#ifdef __APPLE__
#define HANDLER_SECTION "__TEXT,__cc_handlers"
extern CC_handler_map_t __start__cc_handlers __asm("section$start$__TEXT$__cc_handlers");
extern CC_handler_map_t __stop__cc_handlers __asm("section$end$__TEXT$__cc_handlers");
#else
#define HANDLER_SECTION "_cc_handlers"
/**
 * This is the first of the registered app handlers
 */
extern const CC_handler_map_t __start__cc_handlers;
/**
 * This marks the end of the handlers. The element
 * after the last element. This means that this element
 * is not valid.
 */
extern const CC_handler_map_t __stop__cc_handlers;
#endif


/**
 * Every CC must register itself using the REGISTER_CC macro.
 *
 * This macro will locate a variable containing
 * - the CC number,
 * - the CC version, and
 * - the CC handler
 * in a specific code section.
 * This section (array) can then be looped through to access the handler, the version and other
 * stuff that might be added in the future.
 */
#define REGISTER_CC(cc,version,handler) \
  static const CC_handler_map_t thisHandler##cc __attribute__((__used__, __section__( HANDLER_SECTION ))) = {cc,version,handler};

#endif /* ZAF_APPLICATIONUTILITIES_ZAF_TYPES_H_ */
