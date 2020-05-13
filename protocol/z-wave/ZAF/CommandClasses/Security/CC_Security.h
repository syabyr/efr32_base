/**
 * @file
 * Handler for Command Class Security & Command Class Security 2.
 * @copyright 2018 Silicon Laboratories Inc.
 */

#ifndef _CC_SECURITY_H_
#define _CC_SECURITY_H_
#include <ZW_typedefs.h>
#include <ZW_classcmd.h>

/**
 * @brief handleCommandClassSecurity
 * Handler for command class security, command SECURITY_COMMANDS_SUPPORTED_GET
 * @param[in] rxOpt IN receive options of type RECEIVE_OPTIONS_TYPE_EX
 * @param[in] pCmd IN  Payload from the received frame
 * @param[in] cmdLength IN Number of command bytes including the command
 * @return receive frame status.
 */
received_frame_status_t
handleCommandClassSecurity(
  RECEIVE_OPTIONS_TYPE_EX *rxOpt,
  ZW_APPLICATION_TX_BUFFER *pCmd,
  uint8_t cmdLength);


#endif /* _CC_SECURITY_H_ */
