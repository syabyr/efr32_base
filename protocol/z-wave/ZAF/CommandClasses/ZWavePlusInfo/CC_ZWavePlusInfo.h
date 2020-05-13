/**
 * @file
 * Handler for Command Class Z-Wave Plus Info.
 * @copyright 2018 Silicon Laboratories Inc.
 */

#ifndef _COMMAND_CLASS_ZWAVE_PLUS_INFO_H_
#define _COMMAND_CLASS_ZWAVE_PLUS_INFO_H_

#include <ZW_classcmd.h>
#include <ZAF_types.h>

typedef struct
{
  uint16_t installerIconType;
  uint16_t userIconType;
}
SEndpointIcon;

typedef struct
{
  SEndpointIcon const * const pEndpointInfo;
  uint8_t endpointInfoSize;
}
SEndpointIconList;

typedef struct
{
  SEndpointIconList const * const pEndpointIconList;
  uint8_t roleType;
  uint8_t nodeType;
  uint16_t installerIconType;
  uint16_t userIconType;
}
SCCZWavePlusInfo;

/**
 * Initializes the Z-Wave Plus Info command class.
 * @param[in] pZWPlusInfo Pointer to configuration.
 */
void CC_ZWavePlusInfo_Init(SCCZWavePlusInfo const * const pZWPlusInfo);

/**
 * @brief Handler for Z-Wave Plus Info CC.
 * @param[in] rxOpt Pointer to receive options.
 * @param[in] pCmd Pointer to payload from the received frame
 * @param[in] cmdLength Length of the received command given in bytes.
 * @return receive frame status.
 */
received_frame_status_t handleCommandClassZWavePlusInfo(
  RECEIVE_OPTIONS_TYPE_EX *rxOpt,
  ZW_APPLICATION_TX_BUFFER *pCmd,
  uint8_t cmdLength);

#endif
