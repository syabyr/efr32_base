/***************************************************************************//**
 * @brief Weakly defined callbacks for mailbox client.
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

#include PLATFORM_HEADER
#include CONFIGURATION_HEADER
#include "hal/hal.h"
#include "stack/include/ember.h"

#include "mailbox-client.h"

//------------------------------------------------------------------------------
// Weak callbacks definitions

WEAK(void emberAfPluginMailboxClientMessageSubmitCallback(EmberAfMailboxStatus status,
                                                          EmberNodeId mailboxServer,
                                                          EmberNodeId messageDestination,
                                                          uint8_t tag))
{
}

WEAK(void emberAfPluginMailboxClientMessageDeliveredCallback(EmberAfMailboxStatus status,
                                                             EmberNodeId mailboxServer,
                                                             EmberNodeId messageDestination,
                                                             uint8_t tag))
{
}

WEAK(void emberAfPluginMailboxClientCheckInboxCallback(EmberAfMailboxStatus status,
                                                       EmberNodeId mailboxServer,
                                                       EmberNodeId messageSource,
                                                       uint8_t *message,
                                                       EmberMessageLength messageLength,
                                                       uint8_t tag,
                                                       bool moreMessages))
{
}
