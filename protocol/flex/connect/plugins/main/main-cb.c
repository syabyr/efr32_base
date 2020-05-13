/***************************************************************************//**
 * @brief Set of weakly defined callbacks for the main plugin.
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

#include PLATFORM_HEADER
#include CONFIGURATION_HEADER
#include "stack/include/ember.h"

//------------------------------------------------------------------------------
// Weak callbacks definitions

WEAK(void emberAfMainInitCallback(void))
{
}

WEAK(void emberAfMainTickCallback(void))
{
}

WEAK(void emberAfStackStatusCallback(EmberStatus status))
{
}

WEAK(void emberAfIncomingMessageCallback(EmberIncomingMessage *message))
{
}

WEAK(void emberAfIncomingMacMessageCallback(EmberIncomingMacMessage *message))
{
}

WEAK(void emberAfMessageSentCallback(EmberStatus status,
                                     EmberOutgoingMessage *message))
{
}

WEAK(void emberAfMacMessageSentCallback(EmberStatus status,
                                        EmberOutgoingMacMessage *message))
{
}

WEAK(void emberAfChildJoinCallback(EmberNodeType nodeType,
                                   EmberNodeId nodeId))
{
}

WEAK(void emberAfActiveScanCompleteCallback(void))
{
}

WEAK(void emberAfEnergyScanCompleteCallback(int8_t mean,
                                            int8_t min,
                                            int8_t max,
                                            uint16_t variance))
{
}

WEAK(void emberAfMarkApplicationBuffersCallback(void))
{
}

WEAK(void emberAfIncomingBeaconCallback(EmberPanId panId,
                                        EmberNodeId nodeId,
                                        int8_t rssi,
                                        bool permitJoining,
                                        uint8_t payloadLength,
                                        uint8_t *payload))
{
}

WEAK(void emberAfIncomingBeaconExtendedCallback(EmberPanId panId,
                                                EmberMacAddress *source,
                                                int8_t rssi,
                                                bool permitJoining,
                                                uint8_t beaconFieldsLength,
                                                uint8_t *beaconFields,
                                                uint8_t beaconPayloadLength,
                                                uint8_t *beaconPayload))
{
}

WEAK(void emberAfFrequencyHoppingStartClientCompleteCallback(EmberStatus status))
{
}

WEAK(void emberAfRadioNeedsCalibratingCallback(void))
{
  emberCalibrateCurrentChannel();
}

WEAK(bool emberAfStackIdleCallback(uint32_t *idleTimeMs))
{
  return false;
}
