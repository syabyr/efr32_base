/***************************************************************************//**
 * @brief Set of APIs for the bootloader-interface plugin.
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

#ifndef _BOOTLOADER_INTERFACE_H_
#define _BOOTLOADER_INTERFACE_H_

/**************************************************************************//**
 * Return if the bootloader has been initialized.
 *****************************************************************************/
bool emberAfPluginBootloaderInterfaceIsBootloaderInitialized(void);

/**************************************************************************//**
 * Return bootloader version.
 *
 * @param[out] blVersion pointer to store version number.
 *****************************************************************************/
void emberAfPluginBootloaderInterfaceGetVersion(uint16_t *blVersion);

/**************************************************************************//**
 * Initialize bootloader interface as part of your application initialization
 * to ensure the storage mechanism is ready to use.
 *
 * @return TRUE if initialization was successful, FALSE otherwise.
 *****************************************************************************/
bool emberAfPluginBootloaderInterfaceInit(void);

/**************************************************************************//**
 * Set the storage mechanism to its lowerest power state.
 *
 * Call this function when you are finished access the storage mechanism.
 *****************************************************************************/
void emberAfPluginBootloaderInterfaceSleep(void);

/**************************************************************************//**
 * Erase all content of storage spaces.
 *
 * If legacy bootloader is used, all EEPROM content will be erased.
 * If Gecko bootloader is used, all storage slots content will be erased.
 *
 * @return TRUE if erase was successful, FALSE otherwise.
 *****************************************************************************/
bool emberAfPluginBootloaderInterfaceChipErase(void);

/**************************************************************************//**
 * Erase specific slot content.
 *
 * This function erases all contents of the specified slot.
 * This function is only applicable for Gecko bootloader.
 *
 * @param[in] slotId The slot ID to write to
 *
 * @return TRUE if erase was sucessful, FALSE otherwise.
 *****************************************************************************/
bool emberAfPluginBootloaderInterfaceChipEraseSlot(uint32_t slot);

/**************************************************************************//**
 * Validate image within local storage
 *
 * For legacy bootloader, data within the EEPROM will be used for verification.
 * For Gecko  bootloader, slot 0 will be used for verification.
 *
 * @return One of the following:
 *         - Number of pages in a valid image
 *         - 0 for an invalid image
 *         - ::BL_IMAGE_IS_VALID_CONTINUE (-1) to continue to iterate for the final result.
 *****************************************************************************/
uint16_t emberAfPluginBootloaderInterfaceValidateImage(void);

/**************************************************************************//**
 * Verify, install and boot the application in storage
 *
 * For legacy bootloader, data within the EEPROM will be used..
 * For Gecko  bootloader, slot 0 will be used.
 *****************************************************************************/
void emberAfPluginBootloaderInterfaceBootload(void);

/**************************************************************************//**
 * Reads data from the specified raw storage address and length without
 * being restricted to any page size
 *
 * Note: Not all storage implementations support accesses that are
 *       not page aligned, refer to the HalEepromInformationType
 *       structure for more information.
 *
 * @param[in]  startAddress  Address from which to start reading data
 * @param[in]  length        Length of the data to read
 * @param[out] buffer        A pointer to a buffer where data should be read
 *                           into
 *
 *  @return TRUE if read was successful, FALSE otherwise.
 *****************************************************************************/
bool emberAfPluginBootloaderInterfaceRead(uint32_t startAddress,
                                          uint32_t length,
                                          uint8_t *buffer);

/**************************************************************************//**
 * Writes data to the specified raw storage address and length without
 * being restricted to any page size
 *
 * Note: Not all storage implementations support accesses that are
 *       not page aligned, refer to the HalEepromInformationType
 *       structure for more information.
 *
 * Note: Some storage devices require contents to be erased before
 *       new data can be written, and will return an
 *       ::EEPROM_ERR_ERASE_REQUIRED error if write is called on a
 *       location that is not already erased. Refer to the
 *       HalEepromInformationType structure to see if the attached
 *       storage device requires erasing.
 *
 *  @param[in] startAddress  Address to start writing data
 *  @param[in] length        Length of the data to write
 *  @param[in] buffer        A pointer to the buffer of data to write
 *
 *  @return TRUE if write was successful, FALSE otherwise.
 *****************************************************************************/
bool emberAfPluginBootloaderInterfaceWrite(uint32_t startAddress,
                                           uint32_t length,
                                           uint8_t *buffer);

#endif // _BOOTLOADER_INTERFACE_H_
