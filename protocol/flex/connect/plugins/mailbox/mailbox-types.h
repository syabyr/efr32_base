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

#ifndef _MAILBOX_TYPES_H_
#define _MAILBOX_TYPES_H_

/**
 * @addtogroup mailbox_common
 * @brief Types defined for mailbox.
 *
 * Mailbox protocol is designed for devices that can't be online on the network
 * all the time. The most common example for this is a sleepy end device.
 *
 * Mailbox clients and the server can submit messages into the mailbox, which is
 * stored in RAM on the mailbox server. Clients can then query the mailbox
 * server for available messages.
 *
 * The mailbox server will notify clients who submit messages when a message
 * was delivered or when it couldn't be delivered due to an error.
 *
 * Mailbox uses a plugin configurable protocol endpoint, which is 15 by default.
 *
 * The server can also configure the size of the mailbox (in number of packets,
 * 25 by default) and the packet timeout, after which the server drops the
 * message and notifies the source of the error.
 *
 * The mailbox protocol uses standard data messages, so in case of sleepy end
 * devices, it will use the indirect queue. This means that if a sleepy end
 * device sends a request to a mailbox server, the end device should poll for
 * the response.
 *
 * @note Mailbox is not available in MAC mode due to the lack of endpoints.
 *
 * @{
 */

/**
 * @enum EmberAfMailboxStatus
 * @brief Mailbox return status codes.
 */
#ifdef DOXYGEN_SHOULD_SKIP_THIS
enum EmberAfMailboxStatus
#else
typedef uint8_t EmberAfMailboxStatus;
enum
#endif
{
  EMBER_MAILBOX_STATUS_SUCCESS             = 0x00, /**< The generic "no error"
                                                        message.  */
  EMBER_MAILBOX_STATUS_INVALID_CALL        = 0x01, /**< Indicates invalid
                                                        message pointer or
                                                        length. */
  EMBER_MAILBOX_STATUS_BUSY                = 0x02, /**< Indicates that the
                                                        local mailbox
                                                        implementation is busy
                                                        performing a previously
                                                        requested task. */
  EMBER_MAILBOX_STATUS_STACK_ERROR         = 0x03, /**< Indicates that the
                                                        Connect stack returned
                                                        an error to the mailbox
                                                        plugin. */
  EMBER_MAILBOX_STATUS_INVALID_ADDRESS     = 0x04, /**< Indicates that an
                                                        address passed to the
                                                        API was invalid. */
  EMBER_MAILBOX_STATUS_MESSAGE_TOO_LONG    = 0x05, /**< Indicates that the
                                                        message passed to the
                                                        plugin was too long. */
  EMBER_MAILBOX_STATUS_MESSAGE_TABLE_FULL  = 0x06, /**< Indicates that the
                                                        message table on the
                                                        server is full. */
  EMBER_MAILBOX_STATUS_MESSAGE_NO_BUFFERS  = 0x07, /**< Indicates that the
                                                        server was unable to
                                                        allocate memory from the
                                                        heap (See
                                                        @ref memory_buffer for
                                                        details). */
  EMBER_MAILBOX_STATUS_MESSAGE_NO_RESPONSE = 0x08, /**< Indicates that the
                                                        mailbox server did not
                                                        respond to the client's
                                                        request. */
  EMBER_MAILBOX_STATUS_MESSAGE_TIMED_OUT   = 0x09, /**< Indicates that the
                                                        message timed out on the
                                                        mailbox server.*/
  EMBER_MAILBOX_STATUS_MESSAGE_NO_DATA     = 0x0A, /**< Indicates that there are
                                                        no pending messages on
                                                        the server to this
                                                        device*/
};

/** @} // END addtogroup
 */

#endif // _MAILBOX_TYPES_H_
