/**
 * @file
 * This module implements functions used in combination with command class firmware update.
 * @copyright 2018 Silicon Laboratories Inc.
 */

#ifndef _OTA_UTIL_H_
#define _OTA_UTIL_H_

/****************************************************************************/
/*                              INCLUDE FILES                               */
/****************************************************************************/

#include <stdbool.h>
#include <stdint.h>

/****************************************************************************/
/*                     EXPORTED TYPES and DEFINITIONS                       */
/****************************************************************************/

/**
 * Defines for WaitTime field used in commmand = FIRMWARE_UPDATE_MD_STATUS_REPORT.
 * The WaitTime field MUST report the time that is needed before the receiving
 * node again becomes available for communication after the transfer of an image.
 * The unit is the second.
 * This changed to 30 sec. for protocol to extract image etc. Please see TO# 07591.
 */
#ifndef WAITTIME_FWU_SUCCESS
#define WAITTIME_FWU_SUCCESS 30
#endif
#ifndef WAITTIME_FWU_FAIL
#define WAITTIME_FWU_FAIL 2
#endif

/**
 * enum type OTA_STATUS use to
 */
typedef enum _OTA_STATUS_
{
  OTA_STATUS_DONE = 0,
  OTA_STATUS_ABORT = 1,
  OTA_STATUS_TIMEOUT = 2
} OTA_STATUS;

/**
 * Please see description of CC_FirmwareUpdate_Init().
 */
typedef bool (*CC_FirmwareUpdate_start_callback_t)(uint16_t fwId, uint16_t CRC);
typedef void (*CC_FirmwareUpdate_host_write_callback_t)(uint8_t *pData, uint8_t dataLen);
typedef void (*CC_FirmwareUpdate_finish_callback_t)(OTA_STATUS status);

/****************************************************************************/
/*                              EXPORTED DATA                               */
/****************************************************************************/

// Nothing here.

/****************************************************************************/
/*                           EXPORTED FUNCTIONS                             */
/****************************************************************************/



/**
 * Initializes the Firmware Update Meta Data Command Class.
 *
 * This function must be invoked before a firmware update can be initiated.
 *
 * @param[in] pOtaStart Pointer to a function that is invoked when another node initiates a firmware
 *                      update. If the function returns true, the process will continue. If the
 *                      function returns false, the node will report to the initiator that the
 *                      firmware update requires authentication. The function must take two
 *                      arguments where the first one is the firmware ID and the second one is the
 *                      CRC value of the firmware.
 *                      The argument is not required and can be set to NULL. In that case the
 *                      firmware update process will continue without invoking the function.
 *
 * @param[in] pOtaExtWrite Function pointer is called when firmware image data is received for
 *                         external host.
 *                         The function must take two arguments: the first one is a pointer to the
 *                         firmware image data and the second argument is the length of the data.
 *                         The write process ends with the following call: pOtaExtWrite( NULL, 0).
 *                         Can be set to NULL if firmware update is needed for the Z-Wave
 *                         application only.
 *
 * @param[in] pOtaFinish Pointer to a function that is invoked when the firmware update process
 *                       finishes either successfully or with an error. If the device reboots right
 *                       after the firmware update, the function is invoked before the reboot.
 *                       The function must take an argument that is the status of the firmware
 *                       update.
 *                       The argument is not required and can be set to NULL.
 *
 * @param[in] support_activation Lets the application decide whether delayed activation of the
 *                               firmware is supported. If set to true and the Activation bit in
 *                               Request Get is set to 1, the node will not reboot to the new
 *                               firmware image, but will instead wait for an Activation Set
 *                               command.
 * @return 1 if NVM is supported else 0.
 */
bool CC_FirmwareUpdate_Init(
  CC_FirmwareUpdate_start_callback_t pOtaStart,
  CC_FirmwareUpdate_host_write_callback_t pOtaExtWrite,
  CC_FirmwareUpdate_finish_callback_t pOtaFinish,
  bool support_activation);


/**
 * @brief OtaHostFWU_WriteFinish
 * Host call function when finish reading incoming frame. Ota start to get
 * next frame.
 */
void
OtaHostFWU_WriteFinish(void);


/**
 * @brief OtaHostFWU_Status
 * Application call this function when firmware update process is finish. Status
 * of the process is send to the controller.
 * @param[in] userReboot Tell user to reboot host on successfull firmware update.
 * @param[in] status of the process. true - successfull, false - process failed.
 */
void
OtaHostFWU_Status( bool userReboot, bool status );

/**
 * @brief OtaVerifyImage
 * Uses bootloder functionality to verify downloaded image.
 * Side effects: Returns false in case of app version downgrade
 * @return true if there exists a valid image that can be installed by the bootloader.
 */
bool
OtaVerifyImage();

#endif /* _OTA_UTIL_H_ */

