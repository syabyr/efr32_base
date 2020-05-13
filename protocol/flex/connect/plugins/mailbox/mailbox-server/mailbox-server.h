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

#ifndef _MAILBOX_SERVER_H_
#define _MAILBOX_SERVER_H_

#include "mailbox/mailbox-protocol.h"
#include "mailbox/mailbox-types.h"

/**
 * @addtogroup mailbox_server
 * @brief APIs for mailbox server.
 *
 * @copydetails mailbox_common
 *
 * See mailbox-server.h and mailbox-server.c for source code.
 * @{
 */

/** @brief Add a message to the mailbox server queue. The message is stored in
 * the internal queue until the destination node queries the mailbox server node
 * for messages or upon timeout.
 *
 *  @param[in] destination   The node ID of the destination for this
 *    data message.
 *
 *  @param[in] message    A pointer to the message to be enqueued.
 *
 *  @param[in] messageLength  The length in bytes of the message to be enqueued.
 *
 *  @param[in] tag   A tag value which will be returned in the
 *    corresponding @ref emberAfPluginMailboxServerMessageDeliveredCallback()
 *    callback. The application can use to match the callbacks with the call.
 *
 *  @param[in] useSecurity    Set it @b true if the data message should be
 *  sent to the server using security.
 *
 *  @return An ::EmberAfMailboxStatus value of:
 *  - ::EMBER_MAILBOX_STATUS_SUCCESS if the message was successfully added to
 *    the packet queue.
 *  - ::EMBER_MAILBOX_STATUS_INVALID_CALL if the passed message is invalid.
 *  - ::EMBER_MAILBOX_STATUS_INVALID_ADDRESS if the passed destination address
 *    is invalid.
 *  - ::EMBER_MAILBOX_STATUS_MESSAGE_TOO_LONG if the payload size of the passed
 *    message exceeds the maximum allowable payload for the passed transmission
 *    options.
 *  - ::EMBER_MAILBOX_STATUS_MESSAGE_TABLE_FULL if the packet table is already
 *    full.
 *  - ::EMBER_MAILBOX_STATUS_MESSAGE_NO_BUFFERS if not enough memory buffers are
 *    available for storing the message content.
 *
 * @sa emberAfPluginMailboxClientMessageSubmit()
 */
EmberAfMailboxStatus emberAfPluginMailboxServerAddMessage(EmberNodeId destination,
                                                          uint8_t *message,
                                                          EmberMessageLength messageLength,
                                                          uint8_t tag,
                                                          bool useSecurity);

/** @brief Mailbox Server Message Delivered Callback.
 *
 * This callback is invoked at the server when a message submitted locally by
 * the server was successfully delivered or when it timed-out.
 *
 * @param[in] status    An ::EmberAfMailboxStatus value of:
 * - ::EMBER_MAILBOX_STATUS_SUCCESS indicates that the message was successfully
 *   delivered to the final destination.
 * - ::EMBER_MAILBOX_STATUS_MESSAGE_TIMED_OUT indicates that the message
 *   timed-out and was removed from the server queue.
 *
 * @param[in] messageDestination   The node ID of the destination.
 *
 * @param[in] tag   The tag value passed in the
 *    @ref emberAfPluginMailboxServerAddMessage() API.
 *
 * @sa emberAfPluginMailboxClientMessageDeliveredCallback()
 */
void emberAfPluginMailboxServerMessageDeliveredCallback(EmberAfMailboxStatus status,
                                                        EmberNodeId messageDestination,
                                                        uint8_t tag);

/** @} // END addtogroup
 */

#endif //_MAILBOX_SERVER_H_
