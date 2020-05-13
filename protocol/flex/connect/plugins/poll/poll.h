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
 * @addtogroup poll
 * @brief APIs for the poll plugin.
 *
 * The Connect stack supports polling which enables (sleepy) end devices to
 * retrieve pending messages from the parent node (coordinator or range
 * extender).
 *
 * To use this feature, the Poll plugin must be enabled on the end devices.
 * If polling is enabled, the end device sends a data request to the parent node
 * which notifies the device whether there is a pending message or not, using the
 * acknowledge with the pending bit cleared or set. If there is no pending message
 * the communication ends with the acknowledge. If there is a pending message,
 * the parent node sends a data packet containing the pending message which will
 * be acknowledged by the end device.
 *
 * For convenience, Connect supports two polling intervals, long and short, both
 * behaves the same, only the polling period differs. For long polling the period
 * specified in second while in case of short polling the the period is in quarter
 * seconds. The API provides a function to easily switch between the two. Purpose
 * of long polling is maintaining the the connection between the end device and the
 * parent.
 *
 * The application will receive the polled message via the
 * @ref emberIncomingMessageHandler "emberAfIncomingMessageCallback()" function.
 *
 * The poll plugin uses ::emberPollForData() to retrieve the pending message. If the
 * poll plugin is enabled, using ::emberPollForData() is strongly not recommended.
 *
 * See poll.h for source code.
 *
 * @{
 */

/** @brief Set the short poll interval.
 *
 *  @param[in] intervalQS The short poll interval in quarter seconds.
 */
void emberAfPluginPollSetShortPollInterval(uint8_t intervalQS);

/** @brief Set the long poll interval.
 *
 *  @param[in] intervalS The long poll interval in seconds.
 */
void emberAfPluginPollSetLongPollInterval(uint16_t intervalS);

/** @brief Enable/disable short polling.
 *
 *  @param[in] enable If this parameter is true, short polling is enabled.
 *  Otherwise, the node switches back to long polling.
 */
void emberAfPluginPollEnableShortPolling(bool enable);

/** @} // END addtogroup
 */
