/***************************************************************************//**
 * @brief Header for EZSP Host user interface functions
 *
 * See @ref ezsp_util for documentation.
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

/** @addtogroup ezsp_util
 *
 * See ezsp-host-ui.h.
 *
 *@{
 */

#ifndef __EZSP_HOST_UI_H__
#define __EZSP_HOST_UI_H__

/** @brief Prints usage instructions to stderr.
 *
 * @param[in] name  program name (usually argv[0]).
 */
void ezspPrintUsage(char *name);

/** @brief Sets host configuration values from command line options.
 *
 * @param[in] argc  number of command line tokens.
 *
 * @param[in] argv  array of pointer to command line tokens.
 *
 * @return  True if no errors were detected in the command line.
 */
bool ezspProcessCommandOptions(int argc, char *argv[]);

/** @brief Writes a debug trace message, if enabled.
 *
 * @param[in] string  pointer to message string
 *
 * @return
 * - ::EZSP_SUCCESS
 * - ::EZSP_NO_RX_DATA
 */
void ezspTraceEvent(const char *string);

/** @brief  Converts EZSP error code to a string.
 *
 * @param[in] error  error or reset code (from hostError or ncpError).
 *
 * @return  pointer to the string.
 */
const uint8_t* ezspErrorString(uint8_t error);

#ifdef EZSP_ASH
  #define BUMP_HOST_COUNTER(mbr) do { ashCount.mbr++; } while (0)
  #define ADD_HOST_COUNTER(op, mbr) do { ashCount.mbr += op; }  while (0)
  #include "ezsp-host/ash/ash-host.h"
  #include "ezsp-host/ash/ash-host-ui.h"
  #define readConfig(x) ashReadConfig(x)
#elif defined(EZSP_USB)
  #include "ezsp-host/usb/usb-host.h"
  #include "ezsp-host/usb/usb-host-ui.h"
  #define readConfig(x) usbReadConfig(x)
  #define BUMP_HOST_COUNTER(mbr)
  #define ADD_HOST_COUNTER(op, mbr)
#elif defined(EZSP_SPI)
  #include "ezsp-host/spi/spi-host.h"
  #include "ezsp-host/spi/spi-host-ui.h"
  #define readConfig(x) spiReadConfig(x)
  #define BUMP_HOST_COUNTER(mbr)
  #define ADD_HOST_COUNTER(op, mbr)
#endif
#endif //__EZSP_HOST_UI_H___

/** @} // END addtogroup
 */
