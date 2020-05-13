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

#ifndef _MAILBOX_CLIENT_H_
#define _MAILBOX_CLIENT_H_

#include "mailbox/mailbox-protocol.h"
#include "mailbox/mailbox-types.h"

/**
 * @addtogroup mailbox_client
 * @brief APIs for mailbox client.
 *
 * @copydetails mailbox_common
 *
 * See mailbox-client.h and mailbox-client.c for source code.
 * @{
 */

/** @brief Submit a data message to a mailbox server. If this
 * API returns an ::EmberAfMailboxStatus value of
 * ::EMBER_MAILBOX_STATUS_SUCCESS, the corresponding asynchronous callback
 * ::emberAfPluginMailboxClientMessageSubmitCallback() will be invoked to
 * indicate whether the message was successfully submitted to the mailbox
 * server or to inform the application of the reason of failure.
 *
 *  @param[in] mailboxServer   The node ID of the mailbox server.
 *
 *  @param[in] messageDestination   The node ID of the destination for this
 *    data message.
 *
 *  @param[in] message   A pointer to the message to be sent.
 *
 *  @param[in] messageLength   The length in bytes of the message to be sent.
 *
 *  @param[in] tag   A tag value which will be returned in all of the
 *    corresponding callbacks:
 *    @ref emberAfPluginMailboxClientMessageSubmitCallback(),
 *    @ref emberAfPluginMailboxClientMessageDeliveredCallback() and
 *    @ref emberAfPluginMailboxClientCheckInboxCallback(). The application
 *    can use it to match the callbacks with the call.
 *
 *  @param[in] useSecurity   Set it @b true if the data message should be
 *  sent to the server using security.
 *
 *  @return   An ::EmberAfMailboxStatus value of:
 *  - ::EMBER_MAILBOX_STATUS_SUCCESS if the message was successfully passed to
 *    the network layer to be transmitted to the mailbox server.
 *  - ::EMBER_MAILBOX_STATUS_INVALID_CALL if the passed data message is
 *    invalid.
 *  - ::EMBER_MAILBOX_STATUS_INVALID_ADDRESS if the server ID or the
 *    destination ID is an invalid address.
 *  - ::EMBER_MAILBOX_STATUS_MESSAGE_TOO_LONG if the passed message does not
 *    fit in a single mailbox data message.
 *  - ::EMBER_MAILBOX_STATUS_BUSY if the client is still performing a submit
 *    message or a query for message action.
 *  - ::EMBER_MAILBOX_STATUS_STACK_ERROR if the network layer refused the
 *    message (the outgoing queue is currently full).
 *
 *  @note Receiving the ::emberAfPluginMailboxClientMessageSubmitCallback()
 *    requires the reception of a mailbox command message, which is only
 *    possible by polling if the message was submitted on a
 *    @ref EMBER_STAR_SLEEPY_END_DEVICE.
 *  @sa emberAfPluginMailboxServerAddMessage()
 */
EmberAfMailboxStatus emberAfPluginMailboxClientMessageSubmit(EmberNodeId mailboxServer,
                                                             EmberNodeId messageDestination,
                                                             uint8_t *message,
                                                             EmberMessageLength messageLength,
                                                             uint8_t tag,
                                                             bool useSecurity);

/** @brief Query a mailbox server for pending messages. If this
 * API returns an ::EmberAfMailboxStatus value of
 * ::EMBER_MAILBOX_STATUS_SUCCESS, the corresponding asynchronous callback
 * ::emberAfPluginMailboxClientCheckInboxCallback() will be invoked either to
 * provide the retrieved message or to indicate the reason for failure.
 *
 *  @param[in] mailboxServer   The node ID of the mailbox server.
 *
 *  @param[in] useSecurity    Set it @b true if the request command and the
 *    responses to it should be sent secured. If a pending message
 *    was sent to a server securely, it will be always retrieved securely. This
 *    option only affects the request command and the pending messages that were
 *    sent without security to the server.
 *
 *  @return An ::EmberAfMailboxStatus value of:
 *  - ::EMBER_MAILBOX_STATUS_SUCCESS if the query command was successfully
 *    passed to the network layer to be transmitted to the mailbox server.
 *  - ::EMBER_MAILBOX_STATUS_INVALID_ADDRESS if the passed mailbox server short
 *    ID is an invalid address.
 *  - ::EMBER_MAILBOX_STATUS_BUSY if the client is still performing a submit
 *    message or a query for message action.
 *  - ::EMBER_MAILBOX_STATUS_STACK_ERROR if the network layer refused the
 *    command (the outgoing queue is currently full).
 *
 * @note Receiving the ::emberAfPluginMailboxClientCheckInboxCallback()
 *    requires the reception of a mailbox command message, which is
 *    only possible by polling if the message was submitted on a
 *    @ref EMBER_STAR_SLEEPY_END_DEVICE.
 */
EmberAfMailboxStatus emberAfPluginMailboxClientCheckInbox(EmberNodeId mailboxServer,
                                                          bool useSecurity);

/** @brief Mailbox Client Message Submit Callback.
 *
 * A callback invoked when a message arrived to the mailbox server after a call
 * of ::emberAfPluginMailboxClientMessageSubmit().
 *
 *  @param[in] status   An ::EmberAfMailboxStatus value of:
 *  - ::EMBER_MAILBOX_STATUS_SUCCESS if the data message was accepted by the
 *  mailbox server.
 *  - ::EMBER_MAILBOX_STATUS_STACK_ERROR if the message couldn't be delivered
 *  to the mailbox server.
 *  - ::EMBER_MAILBOX_STATUS_MESSAGE_NO_RESPONSE if the client timed-out
 *  waiting for a response from the server.
 *  - ::EMBER_MAILBOX_STATUS_MESSAGE_TABLE_FULL if the mailbox server table is
 *  currently full.
 *  - ::EMBER_MAILBOX_STATUS_MESSAGE_NO_BUFFERS if the server can't
 *  allocate enough memory to store the message.
 *
 *  @param[in] mailboxServer   The node ID of the mailbox server.
 *
 *  @param[in] messageDestination   The node ID of the destination.
 *
 *  @param tag   The tag value passed in the
 *    ::emberAfPluginMailboxClientMessageSubmit() API.
 *
 *  @note Receiving this callback
 *    requires the reception of a mailbox command message, which is only
 *    possible by polling if the message was submitted on a
 *    @ref EMBER_STAR_SLEEPY_END_DEVICE.
 */
void emberAfPluginMailboxClientMessageSubmitCallback(EmberAfMailboxStatus status,
                                                     EmberNodeId mailboxServer,
                                                     EmberNodeId messageDestination,
                                                     uint8_t tag);

/** @brief Mailbox Client Message Delivered Callback.
 *
 * A callback that may be invoked on the submitter of the message either if the
 * message that was submitted to a mailbox server reached its final destination
 * or it timed-out. Note that the callback is not always called. If the status
 * message from the server is lost, the callback won't be called.
 *
 * @param[in] status    An ::EmberAfMailboxStatus value of:
 * - ::EMBER_MAILBOX_STATUS_SUCCESS indicates that the message was successfully
 *   delivered to the final destination.
 * - ::EMBER_MAILBOX_STATUS_MESSAGE_TIMED_OUT indicates that the message
 *   timed-out and was removed from the server queue.
 *
 * @param[in] mailboxServer   The node ID of the mailbox server where the
 *   message was submitted to.
 *
 * @param[in] messageDestination   The node ID of the destination.
 *
 * @param[in] tag   The tag value passed in the
 *    ::emberAfPluginMailboxClientMessageSubmit() API.
 *
 * @note Receiving this callback
 *    requires the reception of a mailbox command message, which is
 *    only possible by polling if the message was submitted on a
 *    @ref EMBER_STAR_SLEEPY_END_DEVICE.
 */
void emberAfPluginMailboxClientMessageDeliveredCallback(EmberAfMailboxStatus status,
                                                        EmberNodeId mailboxServer,
                                                        EmberNodeId messageDestination,
                                                        uint8_t tag);

/** @brief Mailbox Client Check Inbox Callback.
 *
 * This callback is invoked after a successful call to the
 * ::emberAfPluginMailboxClientCheckInbox() API. If a message was retrieved
 * from the mailbox server, this callback passes it to the application.
 * Otherwise, it indicates the reason for failure to the application.
 *
 *  @param[in] status    An ::EmberAfMailboxStatus value of:
 * - ::EMBER_MAILBOX_STATUS_SUCCESS if a message was retrieved from the mailbox
 * server.
 * - ::EMBER_MAILBOX_STATUS_MESSAGE_NO_DATA if the server has currently no
 * message for this mailbox client.
 * - ::EMBER_MAILBOX_STATUS_MESSAGE_NO_RESPONSE if the client timed-out waiting
 * for a query response from the mailbox server.
 * - ::EMBER_MAILBOX_STATUS_STACK_ERROR if the stack failed to deliver the
 * query message to the mailbox server.
 *
 *  @param[in] mailboxServer   The node id of the mailbox server responding.
 *
 *  @param[in] messageSource   The source node ID of the retrieved message.
 *  Note that this parameter is meaningful only if the status parameter has
 *  an ::EmberAfMailboxStatus value of ::EMBER_MAILBOX_STATUS_SUCCESS.
 *
 *  @param[in] message   A pointer to the retrieved message payload. Note that
 *  this parameter is meaningful only if the status parameter has an
 *  ::EmberAfMailboxStatus value of ::EMBER_MAILBOX_STATUS_SUCCESS.
 *
 *  @param[in] messageLength   The length in bytes of the retrieved message
 *  payload. Note that this parameter is meaningful only if the status parameter
 *  has an ::EmberAfMailboxStatus value of ::EMBER_MAILBOX_STATUS_SUCCESS.
 *
 *  @param[in] tag   The tag value passed in the
 *  ::emberAfPluginMailboxClientMessageSubmit() API. Note that this parameter is
 *  meaningful only if the status parameter has  an ::EmberAfMailboxStatus value
 *  of ::EMBER_MAILBOX_STATUS_SUCCESS.
 *
 *  @param[in] moreMessages   This flag is \b true if the mailbox server
 *  has more pending messages for this mailbox client. Note that this parameter
 *  is meaningful only if the status parameter has an ::EmberAfMailboxStatus
 *  value of ::EMBER_MAILBOX_STATUS_SUCCESS.
 *
 *  @note Receiving this callback
 *    requires the reception of a mailbox command message, which is
 *    only possible by polling if the message was submitted on a
 *    @ref EMBER_STAR_SLEEPY_END_DEVICE.
 */
void emberAfPluginMailboxClientCheckInboxCallback(EmberAfMailboxStatus status,
                                                  EmberNodeId mailboxServer,
                                                  EmberNodeId messageSource,
                                                  uint8_t *message,
                                                  EmberMessageLength messageLength,
                                                  uint8_t tag,
                                                  bool moreMessages);

/** @} // END addtogroup
 */

#endif // _MAILBOX_CLIENT_H_
