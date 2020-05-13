/***************************************************************************//**
 * @brief Weakly defined callbacks for ota unicast bootloader servers.
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
#include "hal/hal.h"
#include "stack/include/ember.h"

#include "ota-unicast-bootloader-server.h"
#include "ota-unicast-bootloader-server-internal.h"

//------------------------------------------------------------------------------
// Weak callbacks definitions

WEAK(bool emberAfPluginOtaUnicastBootloaderServerGetImageSegmentCallback(
       uint32_t startIndex,
       uint32_t endIndex,
       uint8_t imageTag,
       uint8_t *imageSegment
       ))
{
  return false;
}

WEAK(void emberAfPluginOtaUnicastBootloaderServerImageDistributionCompleteCallback(
       EmberAfOtaUnicastBootloaderStatus status))
{
}

WEAK(void emberAfPluginOtaUnicastBootloaderServerRequestTargetBootloadCompleteCallback(
       EmberAfOtaUnicastBootloaderStatus status))
{
}
