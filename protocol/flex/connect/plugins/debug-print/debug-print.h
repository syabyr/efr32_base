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

/**
 * @addtogroup debug_print
 * @brief Set of APIs for the debug-print plugin.
 *
 * See debug-print.h for source code.
 * @{
 */
/** @brief Indicate whether a printing area is enabled. */
bool emberAfPrintEnabled(uint16_t area);

/** @brief Enable a printing area. */
void emberAfPrintOn(uint16_t userArea);

/** @brief Disable a printing area. */
void emberAfPrintOff(uint16_t userArea);

/** @brief Enable all printing areas. */
void emberAfPrintAllOn(void);

/** @brief Disable all printing areas. */
void emberAfPrintAllOff(void);

/** @brief Print the status of the printing areas. */
void emberAfPrintStatus(void);

/** @brief Print a formatted message. */
void emberAfPrint(uint16_t area, PGM_P formatString, ...);

/** @brief Print a formatted message followed by a newline. */
void emberAfPrintln(uint16_t area, PGM_P formatString, ...);

/** @brief Prints a buffer as a series of bytes in hexadecimal format. */
void emberAfPrintBuffer(uint16_t area,
                        const uint8_t *buffer,
                        uint16_t bufferLen,
                        bool withSpace);

/** @brief Prints an EUI64 (IEEE address) in big-endian format. */
void emberAfPrintBigEndianEui64(const EmberEUI64 eui64);

/** @brief Prints an EUI64 (IEEE address) in little-endian format. */
void emberAfPrintLittleEndianEui64(const EmberEUI64 eui64);

/** @brief Prints a 16-byte key. */
void emberAfPrintKey(const uint8_t *key);

/** @brief Waits for all data currently queued to be transmitted. */
void emberAfFlush(uint16_t area);

extern uint16_t emberAfPrintActiveArea;

/** @} END addtogroup */
