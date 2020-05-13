/**
 * @file
 * Handler for Command Class Powerlevel.
 * @copyright 2018 Silicon Laboratories Inc.
 */

#ifndef _CC_POWERLEVEL_H_
#define _CC_POWERLEVEL_H_

/****************************************************************************/
/*                              INCLUDE FILES                               */
/****************************************************************************/
#include <ZW_typedefs.h>
#include <ZW_classcmd.h>
#include <ZW_TransportEndpoint.h>
#include "QueueNotifying.h"

/**
 * For backwards compatibility.
 */
#define CommandClassPowerLevelVersionGet()    CC_Powerlevel_getVersion()
#define handleCommandClassPowerLevel(a, b, c) CC_Powerlevel_handler(a, b, c)
#define CommandClassPowerLevelIsInProgress()  CC_Powerlevel_isInProgress()


/**
 * Load parameters from NVM
 */
void loadStatusPowerLevel(void);

 /**
 * loads initial power level status from nvram
 */
void loadInitStatusPowerLevel(void);

/**
 * Handler for Powerlevel CC.
 * @param[in] rxOpt Pointer to receive options.
 * @param[in] pCmd Pointer to payload from the received frame
 * @param[in] cmdLength Length of the received command given in bytes.
 * @return receive frame status.
 */
received_frame_status_t CC_Powerlevel_handler(
  RECEIVE_OPTIONS_TYPE_EX *rxOpt,
  ZW_APPLICATION_TX_BUFFER *pCmd,
  uint8_t cmdLength);

/**
 * Returns whether a powerlevel test is in progress.
 * @return true if in progress, false otherwise.
 */
bool CC_Powerlevel_isInProgress(void);

#endif
