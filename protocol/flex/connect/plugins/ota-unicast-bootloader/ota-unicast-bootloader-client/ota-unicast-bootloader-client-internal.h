/***************************************************************************//**
 * @brief Macros, includes, and types for ota-unicast-bootloader clients.
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

#ifndef _OTA_UNICAST_BOOTLOADER_CLIENT_INTERNAL_H_
#define _OTA_UNICAST_BOOTLOADER_CLIENT_INTERNAL_H_

#include "ota-unicast-bootloader/ota-unicast-bootloader-protocol.h"
#include "ota-unicast-bootloader/ota-unicast-bootloader-common-internal.h"

//------------------------------------------------------------------------------
// Internal states

#define STATE_OTA_CLIENT_IDLE                                               0x00
#define STATE_OTA_CLIENT_IMAGE_DOWNLOAD_WAITING                             0x01

#define clientIsIdle()  (internalState == STATE_OTA_CLIENT_IDLE)
#define clientInImageDownload() \
  (internalState == STATE_OTA_CLIENT_IMAGE_DOWNLOAD_WAITING)

#endif // _OTA_UNICAST_BOOTLOADER_CLIENT_INTERNAL_H_
