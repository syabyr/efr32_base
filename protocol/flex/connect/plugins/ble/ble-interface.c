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
#include CONFIGURATION_HEADER

#include "rtos_bluetooth.h"
#include "gatt_db.h"
#include "hal-config.h"

#include "flex-callbacks.h"

#include <kernel/include/os.h>

#define BLE_HEAP_SIZE DEFAULT_BLUETOOTH_HEAP(EMBER_AF_PLUGIN_BLE_MAX_CONNECTIONS)

#if defined(EMBER_AF_PLUGIN_BLE_PSSTORE_LIBRARY)
// This is needed in order to properly place the PSStore space at the top of
// flash (PSStore is not relocatable, so it needs to be at the top of flash).
VAR_AT_SEGMENT(NO_STRIPPING uint8_t psStore[FLASH_PAGE_SIZE * 2], __PSSTORE__);
#endif

static uint8_t bluetooth_stack_heap[BLE_HEAP_SIZE];

void BluetoothLLCallback();
void BluetoothUpdate();

static gecko_configuration_t config =
{
  .config_flags = GECKO_CONFIG_FLAG_RTOS,
#if defined(EMBER_AF_PLUGIN_MICRIUM_RTOS_SLEEP_ENABLE)
  .sleep.flags = SLEEP_FLAGS_DEEP_SLEEP_ENABLE,
#else
  .sleep.flags = 0,
#endif // EMBER_AF_PLUGIN_MICRIUM_RTOS_SLEEP_ENABLE
  .bluetooth.max_connections = EMBER_AF_PLUGIN_BLE_MAX_CONNECTIONS,
  .bluetooth.heap = bluetooth_stack_heap,
  .bluetooth.heap_size = sizeof(bluetooth_stack_heap),
  .gattdb = &bg_gattdb_data,
  .scheduler_callback = BluetoothLLCallback,
  .stack_schedule_callback = BluetoothUpdate,
  .mbedtls.flags = GECKO_MBEDTLS_FLAGS_NO_MBEDTLS_DEVICE_INIT,
  .mbedtls.dev_number = 0,
#if HAL_PA_ENABLE
  .pa.config_enable = 1,
#if BSP_PA_VOLTAGE > 1800
  .pa.input = GECKO_RADIO_PA_INPUT_VBAT,
#else
  .pa.input = GECKO_RADIO_PA_INPUT_DCDC,
#endif
#endif // HAL_PA_ENABLE
};

gecko_configuration_t *emberAfPluginBleGetConfig(void)
{
  emberAfPluginBleGetConfigCallback(&config);

  // Re-assign the heap-related fields, just in case.
  config.bluetooth.max_connections = EMBER_AF_PLUGIN_BLE_MAX_CONNECTIONS;
  config.bluetooth.heap = bluetooth_stack_heap;
  config.bluetooth.heap_size = sizeof(bluetooth_stack_heap);

  return &config;
}

bool emberAfPluginBleHasEventPending(void)
{
  RTOS_ERR os_err;

  OSFlagPend(&bluetooth_event_flags, (OS_FLAGS)BLUETOOTH_EVENT_FLAG_EVT_WAITING,
             0,
             OS_OPT_PEND_NON_BLOCKING
             + OS_OPT_PEND_FLAG_SET_ANY,
             NULL,
             &os_err);

  return (RTOS_ERR_CODE_GET(os_err) == RTOS_ERR_NONE);
}

// This is run as part of the Application Framework task main loop.
void emberAfPluginBleTickCallback(void)
{
  RTOS_ERR os_err;

  OSFlagPend(&bluetooth_event_flags,
             (OS_FLAGS)BLUETOOTH_EVENT_FLAG_EVT_WAITING,
             0,
             OS_OPT_PEND_NON_BLOCKING
             + OS_OPT_PEND_FLAG_SET_ANY
             + OS_OPT_PEND_FLAG_CONSUME,
             NULL,
             &os_err);

  if (RTOS_ERR_CODE_GET(os_err) == RTOS_ERR_NONE) {
    emberAfPluginBleEventCallback((struct gecko_cmd_packet*)bluetooth_evt);

    OSFlagPost(&bluetooth_event_flags,
               (OS_FLAGS)BLUETOOTH_EVENT_FLAG_EVT_HANDLED,
               OS_OPT_POST_FLAG_SET,
               &os_err);
  }
}
