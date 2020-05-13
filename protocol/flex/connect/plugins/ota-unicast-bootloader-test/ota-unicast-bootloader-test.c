/***************************************************************************//**
 * @brief APIs for the ota-unicast-bootloader-test plugin.
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
#include "hal/hal.h"
#include "debug-print/debug-print.h"
#include "ota-bootloader-test-common/ota-bootloader-test-common.h"

#include EMBER_AF_API_BOOTLOADER_INTERFACE
#include EMBER_AF_API_COMMAND_INTERPRETER2

#if defined(EMBER_AF_PLUGIN_OTA_UNICAST_BOOTLOADER_SERVER)
#include EMBER_AF_API_OTA_UNICAST_BOOTLOADER_SERVER
#endif // EMBER_AF_PLUGIN_OTA_UNICAST_BOOTLOADER_SERVER

#if defined(EMBER_AF_PLUGIN_OTA_UNICAST_BOOTLOADER_CLIENT)
#include EMBER_AF_API_OTA_UNICAST_BOOTLOADER_CLIENT
#endif // EMBER_AF_PLUGIN_OTA_UNICAST_BOOTLOADER_CLIENT

EmberEventControl emAfPluginOtaUnicastBootloaderTestEventControl;

static EmberNodeId target;

static bool otaResumeEnable = true;
static uint32_t unicastDownloadStartIndex = 0;

//------------------------------------------------------------------------------
// Event code (used to schedule a bootload action at the client).

void bootloaderFlashImage(void);

void emAfPluginOtaUnicastBootloaderTestEventHandler(void)
{
  emberEventControlSetInactive(emAfPluginOtaUnicastBootloaderTestEventControl);

  bootloaderFlashImage();
}

//------------------------------------------------------------------------------
// OTA Bootloader Server implemented callbacks.

#if defined(EMBER_AF_PLUGIN_OTA_UNICAST_BOOTLOADER_SERVER)

bool emberAfPluginOtaUnicastBootloaderServerGetImageSegmentCallback(uint32_t startIndex,
                                                                    uint32_t endIndex,
                                                                    uint8_t imageTag,
                                                                    uint8_t *imageSegment)
{
  emberAfCorePrintln("(server): get segment, start: %d, end: %d, tag: 0x%x",
                     startIndex, endIndex, imageTag);

  //Initialize bootloader (and flash part) if not yet initialized or in shutdown.
  if ( !emberAfPluginBootloaderInterfaceIsBootloaderInitialized() ) {
    if ( !emberAfPluginBootloaderInterfaceInit() ) {
      return false;
    }
  }

  if ( !emberAfPluginBootloaderInterfaceRead(startIndex, endIndex - startIndex + 1, imageSegment) ) {
    return false;
  }

  emberAfCorePrint(".");

  return true;
}

void emberAfPluginOtaUnicastBootloaderServerImageDistributionCompleteCallback(EmberAfOtaUnicastBootloaderStatus status)
{
  emberAfCorePrintln("image distribution completed, 0x%x", status);
}

void emberAfPluginOtaUnicastBootloaderServerRequestTargetBootloadCompleteCallback(EmberAfOtaUnicastBootloaderStatus status)
{
  emberAfCorePrintln("bootload request completed, 0x%x", status);
}

#endif // EMBER_AF_PLUGIN_OTA_UNICAST_BOOTLOADER_SERVER

//------------------------------------------------------------------------------
// OTA Unicast Bootloader Client implemented callbacks.

#if defined(EMBER_AF_PLUGIN_OTA_UNICAST_BOOTLOADER_CLIENT)

bool emberAfPluginOtaUnicastBootloaderClientNewIncomingImageCallback(EmberNodeId serverId,
                                                                     uint8_t imageTag,
                                                                     uint32_t imageSize,
                                                                     uint32_t *startIndex)
{
  // The client shall accept images with matching tag
  bool accept = (imageTag == otaBootloaderTestImageTag);

  if (otaResumeEnable) {
    if (otaResumeStartCounterReset) {
      *startIndex = 0;
      otaResumeStartCounterReset = false;
    } else {
      *startIndex = unicastDownloadStartIndex;
    }
  }
  emberAfCorePrintln("new incoming unicast image %s (tag=0x%x)",
                     ((accept) ? "ACCEPTED" : "REFUSED"),
                     imageTag);

  return accept;
}

void emberAfPluginOtaUnicastBootloaderClientIncomingImageSegmentCallback(EmberNodeId serverId,
                                                                         uint32_t startIndex,
                                                                         uint32_t endIndex,
                                                                         uint8_t imageTag,
                                                                         uint8_t *imageSegment)
{
  emberAfCorePrintln("(client): incoming segment, start: %d, end: %d, tag: 0x%x",
                     startIndex, endIndex, imageTag);

  //Initialize bootloader (and flash part) if not yet initialized or in shutdown.
  if ( !emberAfPluginBootloaderInterfaceIsBootloaderInitialized() ) {
    if ( !emberAfPluginBootloaderInterfaceInit() ) {
      emberAfCorePrintln("init failed");

      emberAfPluginOtaUnicastBootloaderClientAbortImageDownload(imageTag);

      unicastDownloadStartIndex = 0;
      return;
    }
  }

  if ( !emberAfPluginBootloaderInterfaceWrite(startIndex,
                                              endIndex - startIndex + 1,
                                              imageSegment) ) {
    emberAfCorePrintln("write failed");

    emberAfPluginOtaUnicastBootloaderClientAbortImageDownload(imageTag);

    unicastDownloadStartIndex = 0;
    return;
  }

  unicastDownloadStartIndex = endIndex + 1;

  emberAfCorePrint(".");
}

void emberAfPluginOtaUnicastBootloaderClientImageDownloadCompleteCallback(EmberAfOtaUnicastBootloaderStatus status,
                                                                          uint8_t imageTag,
                                                                          uint32_t imageSize)
{
  if (status == EMBER_OTA_UNICAST_BOOTLOADER_STATUS_SUCCESS) {
    emberAfCorePrintln("Image download COMPLETED tag=0x%x size=%d",
                       imageTag, imageSize);
    unicastDownloadStartIndex = 0;
  } else {
    emberAfCorePrintln("Image download FAILED status=0x%x", status);
  }
}

bool emberAfPluginOtaUnicastBootloaderClientIncomingRequestBootloadCallback(EmberNodeId serverId,
                                                                            uint8_t imageTag,
                                                                            uint32_t bootloadDelayMs)
{
  // The client shall bootload an image with matching tag.

  // TODO: we should also maintain a state machine to keep track whether this
  // image was previously correctly received. For now we assume the server would
  // issue a bootload request only if the client reported a successful image
  // download.
  bool accept = (imageTag == otaBootloaderTestImageTag);

  if (accept) {
    emberAfCorePrintln("bootload request for image with tag 0x%x accepted, will bootload in %d ms",
                       imageTag, bootloadDelayMs);
    // Schedule a bootload action.
    emberEventControlSetDelayMS(emAfPluginOtaUnicastBootloaderTestEventControl,
                                bootloadDelayMs);
  } else {
    emberAfCorePrintln("bootload request refused (tag 0x%x doesn't match)",
                       imageTag);
  }

  return accept;
}

#endif // EMBER_AF_PLUGIN_OTA_UNICAST_BOOTLOADER_CLIENT

//------------------------------------------------------------------------------
// CLI commands.

void otaUnicastBootloaderSetTarget(void)
{
  EmberNodeId targetId = emberUnsignedCommandArgument(0);

  target = targetId;
  emberAfCorePrintln("unicast target set");
}

void otaUnicastBootloaderStartDistribution(void)
{
#if defined(EMBER_AF_PLUGIN_OTA_UNICAST_BOOTLOADER_SERVER)
  uint32_t imageSize = emberUnsignedCommandArgument(0);
  uint8_t imageTag = emberUnsignedCommandArgument(1);

  EmberAfOtaUnicastBootloaderStatus status =
    emberAfPluginOtaUnicastBootloaderServerInitiateImageDistribution(target,
                                                                     imageSize,
                                                                     imageTag);
  if (status == EMBER_OTA_UNICAST_BOOTLOADER_STATUS_SUCCESS) {
    emberAfCorePrintln("unicast image distribution initiated");
  } else {
    emberAfCorePrintln("unicast image distribution failed 0x%x", status);
  }
#else
  emberAfCorePrintln("OTA unicast bootloader server plugin not included");
#endif // EMBER_AF_PLUGIN_OTA_UNICAST_BOOTLOADER_SERVER
}

void otaUnicastBootloaderRequestBootload(void)
{
#if defined(EMBER_AF_PLUGIN_OTA_UNICAST_BOOTLOADER_SERVER)
  uint32_t delayMs = emberUnsignedCommandArgument(0);
  uint8_t imageTag = emberUnsignedCommandArgument(1);

  EmberAfOtaUnicastBootloaderStatus status =
    emberAfPluginUnicastBootloaderServerInitiateRequestTargetBootload(delayMs,
                                                                      imageTag,
                                                                      target);
  if (status == EMBER_OTA_UNICAST_BOOTLOADER_STATUS_SUCCESS) {
    emberAfCorePrintln("bootload request initiated");
  } else {
    emberAfCorePrintln("bootload request failed 0x%x", status);
  }
#else
  emberAfCorePrintln("OTA bootloader server plugin not included");
#endif // EMBER_AF_PLUGIN_OTA_UNICAST_BOOTLOADER_SERVER
}
