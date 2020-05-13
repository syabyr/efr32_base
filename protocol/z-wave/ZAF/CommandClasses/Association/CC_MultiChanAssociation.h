/**
 * @file
 * Handler for Command Class Multi Channel Association.
 * @copyright 2018 Silicon Laboratories Inc.
 */

#ifndef _CC_MULTICHANASSOCIATION_H_
#define _CC_MULTICHANASSOCIATION_H_

/****************************************************************************/
/*                              INCLUDE FILES                               */
/****************************************************************************/

#include <ZW_TransportEndpoint.h>

/****************************************************************************/
/*                     EXPORTED TYPES and DEFINITIONS                       */
/****************************************************************************/

// Nothing here.

/****************************************************************************/
/*                              EXPORTED DATA                               */
/****************************************************************************/

// Nothing here.

/****************************************************************************/
/*                           EXPORTED FUNCTIONS                             */
/****************************************************************************/

/**
 * @brief Handler for the Multi Channel Association Command Class.
 * @param[in] rxOpt Receive options.
 * @param[in] pCmd Payload from the received frame.
 * @param[in] cmdLength Length of the given payload.
 * @return receive frame status.
 */
received_frame_status_t handleCommandClassMultiChannelAssociation(
    RECEIVE_OPTIONS_TYPE_EX *rxOpt,
    ZW_APPLICATION_TX_BUFFER *pCmd,
    uint8_t cmdLength);

#endif /* _CC_MULTICHANASSOCIATION_H_ */
