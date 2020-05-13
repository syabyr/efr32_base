/***************************************************************************//**
 * @brief Bluetooth stack interface
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
#include "rtos_bluetooth.h"
#include "gatt_db.h"

//------------------------------------------------------------------------------
// Weak callbacks definitions

WEAK(void emberAfPluginBleGetConfigCallback(gecko_configuration_t* config))
{
}

WEAK(void emberAfPluginBleEventCallback(struct gecko_cmd_packet* evt))
{
}
