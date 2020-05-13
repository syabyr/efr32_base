/**
 * @file
 * Handler for Command Class Multi Channel.
 * @copyright 2018 Silicon Laboratories Inc.
 */

#ifndef _CMULTICHAN_H_
#define _CMULTICHAN_H_

#include "config_app.h"
#include <ZW_basis_api.h>
#include <CC_Common.h>
#include <ZW_TransportEndpoint.h>


/****************************************************************************/
/*                     EXPORTED TYPES and DEFINITIONS                       */
/****************************************************************************/

// Nothing here.

/****************************************************************************/
/*                              EXPORTED DATA                               */
/****************************************************************************/


/**
 * Handler for multi channel commands.
 * @param[in] rxOpt Frame header info
 * @param[in] pCmd Payload from the received frame
 * @param[in] cmdLength Number of command bytes including the command
 */
received_frame_status_t MultiChanCommandHandler(
  RECEIVE_OPTIONS_TYPE_EX *rxOpt,
  ZW_APPLICATION_TX_BUFFER *pCmd,
  uint8_t cmdLength);


#endif /* _CMULTICHAN_H_ */
