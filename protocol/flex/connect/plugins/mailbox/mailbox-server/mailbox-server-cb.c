/***************************************************************************//**
 * @brief Weakly defined callbacks for mailbox server.
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

#include "mailbox-server.h"

//------------------------------------------------------------------------------
// Weak callbacks definitions

WEAK(void emberAfPluginMailboxServerMessageDeliveredCallback(EmberAfMailboxStatus status,
                                                             EmberNodeId messageDestination,
                                                             uint8_t tag))
{
}
