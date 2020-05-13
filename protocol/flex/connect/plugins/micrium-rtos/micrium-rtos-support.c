/***************************************************************************//**
 * @brief Micrium RTOS support code.
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

#include "stack/include/api-rename.h"
#include "stack/include/ember.h"

#include <kernel/include/os.h>
#include <kernel/include/os_trace.h>

#if defined(EMBER_AF_PLUGIN_BLE)
#include EMBER_AF_API_BLE
#endif // EMBER_AF_PLUGIN_BLE

#include "micrium-rtos-support.h"
#include "flex-bookkeeping.h"
#include "flex-callbacks.h"

#if defined(EMBER_AF_PLUGIN_BLE)
#include <common/include/rtos_utils.h>
#include "rtos_utils.h"
#endif // EMBER_AF_PLUGIN_BLE

#if defined(EMBER_AF_PLUGIN_MBEDTLS)
#include "mbedtls/threading.h"
#endif

//------------------------------------------------------------------------------
// Tasks variables and defines

#define BLE_LINK_LAYER_TASK_PRIORITY            4
#define BLE_STACK_TASK_PRIORITY                 5
#define CONNECT_STACK_TASK_PRIORITY             6
#define APP_FRAMEWORK_TASK_PRIORITY             7

static OS_TCB connectTaskControlBlock;
static CPU_STK connectTaskStack[EMBER_AF_PLUGIN_MICRIUM_RTOS_CONNECT_STACK_SIZE];

static OS_TCB appFrameworkControlBlock;
static CPU_STK appFrameworkStack[EMBER_AF_PLUGIN_MICRIUM_RTOS_APP_FRAMEWORK_STACK_SIZE];

#if defined(EMBER_AF_PLUGIN_MICRIUM_RTOS_APP_TASK1)
static OS_TCB applicationTask1ControlBlock;
static CPU_STK applicationTask1Stack[EMBER_AF_PLUGIN_MICRIUM_RTOS_APP_TASK1_STACK_SIZE];
#endif // EMBER_AF_PLUGIN_MICRIUM_RTOS_APP_TASK1

#if defined(EMBER_AF_PLUGIN_MICRIUM_RTOS_APP_TASK2)
static OS_TCB applicationTask2ControlBlock;
static CPU_STK applicationTask2Stack[EMBER_AF_PLUGIN_MICRIUM_RTOS_APP_TASK2_STACK_SIZE];
#endif // EMBER_AF_PLUGIN_MICRIUM_RTOS_APP_TASK2

#if defined(EMBER_AF_PLUGIN_MICRIUM_RTOS_APP_TASK3)
static OS_TCB applicationTask3ControlBlock;
static CPU_STK applicationTask3Stack[EMBER_AF_PLUGIN_MICRIUM_RTOS_APP_TASK3_STACK_SIZE];
#endif // EMBER_AF_PLUGIN_MICRIUM_RTOS_APP_TASK3

static OS_MUTEX bufferSystemMutex;

//------------------------------------------------------------------------------
// Forward and external declarations.

extern void App_OS_SetAllHooks(void);

extern OS_FLAG_GRP emAfPluginMicriumRtosFlags;

//------------------------------------------------------------------------------
// Public APIs.

void emberAfPluginMicriumRtosInitAndRunTasks(void)
{
  RTOS_ERR err;

  CPU_Init();
  OS_TRACE_INIT();
  OSInit(&err);
  assert(RTOS_ERR_CODE_GET(err) == RTOS_ERR_NONE);

  App_OS_SetAllHooks();

  // Create Connect task.
  OSTaskCreate(&connectTaskControlBlock,
               "Connect Stack",
               emAfPluginMicriumRtosStackTask,
               NULL,
               CONNECT_STACK_TASK_PRIORITY,
               &connectTaskStack[0],
               EMBER_AF_PLUGIN_MICRIUM_RTOS_CONNECT_STACK_SIZE / 10,
               EMBER_AF_PLUGIN_MICRIUM_RTOS_CONNECT_STACK_SIZE,
               0, // Not receiving messages
               0, // Default time quanta
               NULL, // No TCB extensions
               OS_OPT_TASK_STK_CLR | OS_OPT_TASK_STK_CHK,
               &err);
  assert(RTOS_ERR_CODE_GET(err) == RTOS_ERR_NONE);

  // Start the OS
  OSStart(&err);
  assert(RTOS_ERR_CODE_GET(err) == RTOS_ERR_NONE);
}

void emberAfPluginMicriumRtosAcquireBufferSystemMutex(void)
{
  RTOS_ERR err;

  OSMutexPend((OS_MUTEX *)&bufferSystemMutex,
              (OS_TICK)0,
              (OS_OPT)OS_OPT_PEND_BLOCKING,
              (CPU_TS*)NULL,
              &err);
  assert(RTOS_ERR_CODE_GET(err) == RTOS_ERR_NONE);
}

void emberAfPluginMicriumRtosReleaseBufferSystemMutex(void)
{
  RTOS_ERR err;

  OSMutexPost((OS_MUTEX *)&bufferSystemMutex,
              (OS_OPT)OS_OPT_POST_NONE,
              &err);
  assert(RTOS_ERR_CODE_GET(err) == RTOS_ERR_NONE);
}

//------------------------------------------------------------------------------
// Internal APIs

OS_TCB *emAfPluginMicriumRtosGetStackTcb(void)
{
  return &connectTaskControlBlock;
}

#if defined(EMBER_AF_PLUGIN_BLE)
/***************************************************************************//**
 * Setup the bluetooth init function.
 *
 * @return error code for the gecko_init function
 *
 * All bluetooth specific initialization code should be here like gecko_init(),
 * gecko_init_whitelisting(), gecko_init_multiprotocol() and so on.
 ******************************************************************************/
static errorcode_t initialize_bluetooth()
{
  errorcode_t err = gecko_init(emberAfPluginBleGetConfig());
  APP_RTOS_ASSERT_DBG((err == bg_err_success), 1);
  if (err == bg_err_success) {
    gecko_init_multiprotocol(NULL);
  }
  BluetoothSetWakeupCallback(emAfPluginMicriumRtosWakeUpAppFrameworkTask);
  return err;
}
#endif // EMBER_AF_PLUGIN_BLE

void emAfPluginMicriumRtosInitTasks(void)
{
  RTOS_ERR err;

  OSMutexCreate(&bufferSystemMutex,
                "Buffer system mutex",
                &err);
  assert(RTOS_ERR_CODE_GET(err) == RTOS_ERR_NONE);

  emAfPluginMicriumRtosIpcInit();

#if defined(EMBER_AF_PLUGIN_MBEDTLS)
  THREADING_setup();
#endif

  // Create App Framework task.
  OSTaskCreate(&appFrameworkControlBlock,
               "App Framework",
               emAfPluginMicriumRtosAppFrameworkTask,
               NULL,
               APP_FRAMEWORK_TASK_PRIORITY,
               &appFrameworkStack[0],
               EMBER_AF_PLUGIN_MICRIUM_RTOS_APP_FRAMEWORK_STACK_SIZE / 10,
               EMBER_AF_PLUGIN_MICRIUM_RTOS_APP_FRAMEWORK_STACK_SIZE,
               0, // Not receiving messages
               0, // Default time quanta
               NULL, // No TCB extensions
               OS_OPT_TASK_STK_CLR | OS_OPT_TASK_STK_CHK,
               &err);
  assert(RTOS_ERR_CODE_GET(err) == RTOS_ERR_NONE);

#if defined(EMBER_AF_PLUGIN_BLE)
  bluetooth_start(BLE_LINK_LAYER_TASK_PRIORITY,
                  BLE_STACK_TASK_PRIORITY,
                  initialize_bluetooth);

#endif // EMBER_AF_PLUGIN_BLE

#if defined(EMBER_AF_PLUGIN_MICRIUM_RTOS_APP_TASK1)

  emberAfPluginMicriumRtosAppTask1InitCallback();

  // Create Application Task 1.
  OSTaskCreate(&applicationTask1ControlBlock,
               "Application (1)",
               emberAfPluginMicriumRtosAppTask1MainLoopCallback,
               NULL,
               EMBER_AF_PLUGIN_MICRIUM_RTOS_APP_TASK1_PRIORITY,
               &applicationTask1Stack[0],
               EMBER_AF_PLUGIN_MICRIUM_RTOS_APP_TASK1_STACK_SIZE / 10,
               EMBER_AF_PLUGIN_MICRIUM_RTOS_APP_TASK1_STACK_SIZE,
               0, // Not receiving messages
               0, // Default time quanta
               NULL, // No TCB extensions
               OS_OPT_TASK_STK_CLR | OS_OPT_TASK_STK_CHK,
               &err);
  assert(RTOS_ERR_CODE_GET(err) == RTOS_ERR_NONE);

#endif // EMBER_AF_PLUGIN_MICRIUM_RTOS_APP_TASK1

#if defined(EMBER_AF_PLUGIN_MICRIUM_RTOS_APP_TASK2)

  emberAfPluginMicriumRtosAppTask2InitCallback();

  // Create Application Task 2.
  OSTaskCreate(&applicationTask2ControlBlock,
               "Application (2)",
               emberAfPluginMicriumRtosAppTask2MainLoopCallback,
               NULL,
               EMBER_AF_PLUGIN_MICRIUM_RTOS_APP_TASK2_PRIORITY,
               &applicationTask2Stack[0],
               EMBER_AF_PLUGIN_MICRIUM_RTOS_APP_TASK2_STACK_SIZE / 10,
               EMBER_AF_PLUGIN_MICRIUM_RTOS_APP_TASK2_STACK_SIZE,
               0, // Not receiving messages
               0, // Default time quanta
               NULL, // No TCB extensions
               OS_OPT_TASK_STK_CLR | OS_OPT_TASK_STK_CHK,
               &err);
  assert(RTOS_ERR_CODE_GET(err) == RTOS_ERR_NONE);

#endif // EMBER_AF_PLUGIN_MICRIUM_RTOS_APP_TASK2

#if defined(EMBER_AF_PLUGIN_MICRIUM_RTOS_APP_TASK3)

  emberAfPluginMicriumRtosAppTask3InitCallback();

  // Create Application Task 3.
  OSTaskCreate(&applicationTask3ControlBlock,
               "Application (3)",
               emberAfPluginMicriumRtosAppTask3MainLoopCallback,
               NULL,
               EMBER_AF_PLUGIN_MICRIUM_RTOS_APP_TASK3_PRIORITY,
               &applicationTask3Stack[0],
               EMBER_AF_PLUGIN_MICRIUM_RTOS_APP_TASK3_STACK_SIZE / 10,
               EMBER_AF_PLUGIN_MICRIUM_RTOS_APP_TASK3_STACK_SIZE,
               0, // Not receiving messages
               0, // Default time quanta
               NULL, // No TCB extensions
               OS_OPT_TASK_STK_CLR | OS_OPT_TASK_STK_CHK,
               &err);
  assert(RTOS_ERR_CODE_GET(err) == RTOS_ERR_NONE);

#endif // EMBER_AF_PLUGIN_MICRIUM_RTOS_APP_TASK3
}

//------------------------------------------------------------------------------
// Implemented callbacks

void emberAfPluginMicriumRtosStackIsr(void)
{
  emAfPluginMicriumRtosWakeUpConnectStackTask();
}

void emAcquireBufferSystemMutexHandler(void)
{
  emberAfPluginMicriumRtosAcquireBufferSystemMutex();
}

void emReleaseBufferSystemMutexHandler(void)
{
  emberAfPluginMicriumRtosReleaseBufferSystemMutex();
}

void emAfPluginMicriumStackIsrHandler(void)
{
  emAfPluginMicriumRtosWakeUpConnectStackTask();
}

// This should not fire when running within an OS.
bool emAfPluginMicriumStackIdleHandler(uint32_t *idleTimeMs)
{
  assert(0);
  return false;
}

void halPendSvIsr(void)
{
  PendSV_Handler();
}

void halSysTickIsr(void)
{
  SysTick_Handler();
}
