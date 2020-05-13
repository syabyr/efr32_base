/***************************************************************************//**
 * @file
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

#ifndef _OTA_BROADCAST_BOOTLOADER_CLIENT_H_
#define _OTA_BROADCAST_BOOTLOADER_CLIENT_H_

#include "ota-broadcast-bootloader/ota-broadcast-bootloader-protocol.h"
#include "ota-broadcast-bootloader/ota-broadcast-bootloader-types.h"

/**
 * @addtogroup ota_bootloader_client
 * @brief Set of APIs for ota-broadcast-bootloader-client.
 *
 * @copydetails ota_bootloader_common
 *
 * See ota-broadcast-bootloader-client.h and ota-broadcast-bootloader-client.c
 * for source code.
 * @{
 */

/** @brief  Abort an ongoing image download process.
 *
 * @param[in]   imageTag    A 1-byte tag that identifies the image the client
 * should no longer download.
 *
 * @param[in] applicationErrorStatus   A 1-byte error code reported
 * to the server.
 *
 * @return   An ::EmberAfOtaBootloaderStatus value of:
 * - ::EMBER_OTA_BROADCAST_BOOTLOADER_STATUS_SUCCESS   If the ongoing image
 * download process was successfully aborted.
 * - ::EMBER_OTA_BROADCAST_BOOTLOADER_STATUS_INVALID_CALL   If the client was
 * not currently involved in an image download process or it was currently
 * downloading an image with a different tag.
 */
EmberAfOtaBootloaderStatus emberAfPluginOtaBootloaderClientAbortImageDownload(uint8_t imageTag,
                                                                              uint8_t applicationErrorStatus);

//------------------------------------------------------------------------------
// Callbacks

/** @brief  A callback invoked when the OTA Bootloader Client starts
 * receiving a new image. The application can choose to start receiving the
 * image or it can ignore it. If the application chooses to receive the image,
 * other images sent out by other servers are ignored until the client completes
 * this download.
 *
 * @param[in]   serverId    The node ID of the server that initiated the new
 * image distribution process.
 *
 * @param[out]   alternateServerId   This node ID can be set by the application
 * to include a well-known alternate server. If this is set to a valid address,
 * the client allows segments also from this alternate server. If this is
 * set to ::EMBER_BROADCAST_ADDRESS, the client accepts segments with the
 * same image tag from any server.
 *
 * @param[in]   imageTag    A 1-byte tag that identifies the incoming image.
 *
 * @return  Return @b true to accept the image or @b false to ignore it.
 */
bool emberAfPluginOtaBootloaderClientNewIncomingImageCallback(EmberNodeId serverId,
                                                              EmberNodeId *alternateServerId,
                                                              uint8_t imageTag);

/** @brief  A callback invoked when an image
 * segment, that is part of an image that the application elected to download,
 * was received on the OTA Bootloader Client.
 *
 * @param[in]   serverId    The node ID of the server that initiated the image
 * distribution process.
 *
 * @param[in]   startIndex    The index of the first byte of the passed segment.
 *
 * @param[in]   endIndex   The index of the last byte of the passed segment.
 *
 * @param[in]   imageTag   A 1-byte tag of the image the passed segment belongs
 * to.
 *
 * @param[in]   imageSegment    An array containing the image segment.
 */
void emberAfPluginOtaBootloaderClientIncomingImageSegmentCallback(EmberNodeId serverId,
                                                                  uint32_t startIndex,
                                                                  uint32_t endIndex,
                                                                  uint8_t imageTag,
                                                                  uint8_t *imageSegment);

/** @brief  A callback invoked on an OTA Bootloader Client to indicate
 * that an image downlaod is completed.
 *
 * @param[in] status   An ::EmberAfOtaBootloaderStatus value of:
 * - ::EMBER_OTA_BROADCAST_BOOTLOADER_STATUS_SUCCESS  indicating that the full
 * image corresponding to the passed tag has been received. If this is the case,
 * the client previously handed all the image segments to the application using
 * the ::emberAfPluginOtaBootloaderClientIncomingImageSegmentCallback()
 * callback.
 * - ::EMBER_OTA_BROADCAST_BOOTLOADER_STATUS_FAILED  indicating that the client
 * failed to fully download the image and the download process was terminated.
 * - ::EMBER_OTA_BROADCAST_BOOTLOADER_STATUS_TIMEOUT indicating that the client
 * timed out waiting for a message from the server.
 * - ::EMBER_OTA_BROADCAST_BOOTLOADER_STATUS_ABORTED indicating that the
 * application aborted the ongoing image download process as result of calling
 * the API ::emberAfPluginOtaBootloaderClientAbortImageDownload().
 *
 * @param[in] imageTag   A 1-byte tag of the image this callback refers to.
 *
 * @param[in] imageSize  The total size of the downloaded image in bytes. This
 * parameter is meaningful only in case the status parameter is set to
 * ::EMBER_OTA_BROADCAST_BOOTLOADER_STATUS_SUCCESS.
 */
void emberAfPluginOtaBootloaderClientImageDownloadCompleteCallback(EmberAfOtaBootloaderStatus status,
                                                                   uint8_t imageTag,
                                                                   uint32_t imageSize);

/** @brief  A callback invoked on the OTA Bootloader Client to indicate
 * that an OTA Bootloader Server has requested the status of the client device.
 *
 * @param[in]  serverId   The ID of the server the request came from.
 *
 * @param[in]  applicationServerStatus   The server application status, which was
 * set by @ref emberAfPluginBootloaderServerInitiateRequestTargetsStatus()
 *
 * @param[out] applicationStatus   A 1-byte status set by the client
 * application that is reported to the server.
 */
void emberAfPluginOtaBootloaderClientIncomingRequestStatusCallback(EmberNodeId serverId,
                                                                   uint8_t applicationServerStatus,
                                                                   uint8_t *applicationStatus);

/** @brief  A callback invoked by the OTA Bootloader Client plugin to indicate
 * that an OTA Bootloader Server has requested to perform a bootload operation
 * at a certain point in time in the future.
 *
 * @param[in]  serverId   The ID of the server the request came from.
 *
 * @param[in]  imageTag   A 1-byte tag of the image this callback refers to.
 *
 * @param[in]  bootloadDelayMs   The delay in milliseconds after which the
 * client has been requested to perform a bootload operation.
 *
 * @param[out] applicationStatus   A 1-byte status set by the client application
 * that is reported to the server.
 *
 * @return Return @b true if the application accepted the request of bootloading the
 * specified image at the requested time, @b false otherwise.
 */
bool emberAfPluginOtaBootloaderClientIncomingRequestBootloadCallback(EmberNodeId serverId,
                                                                     uint8_t imageTag,
                                                                     uint32_t bootloadDelayMs,
                                                                     uint8_t *applicationStatus);

/** @} // END addtogroup
 */

#endif // _OTA_BROADCAST_BOOTLOADER_CLIENT_H_
