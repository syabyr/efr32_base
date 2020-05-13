/**
 * @file
 * Functions for system startup on FreeRTOS
 * @copyright 2019 Silicon Laboratories Inc.
 */
#ifndef _ZW_SYSTEM_STARTUP_H_
#define _ZW_SYSTEM_STARTUP_H_

#include <ZW_typedefs.h>
#include "ZW_application_transport_interface.h"


/**
 * Was the device woken up by an RTCC timer interrupt?
 *
 * This function can safely be called from multiple threads.
 *
 * @return true if woken up by RTCC timeout, false otherwise.
 */
bool IsWakeupCausedByRtccTimeout(void);

/**
 * Get number of milliseconds the device has spent sleeping
 * in EM4 before it was woken up.
 *
 * This function can safely be called from multiple threads.

 * @return Number of milliseconds.
 */
uint32_t GetCompletedSleepDurationMs(void);

/**
 * Register a task for the application
 * @param appTaskFunc Application task that will be triggered by FreeRTOS
 * @param iZwRxQueueTaskNotificationBitNumber Set by Protocol when a message has been put on the ZW Rx queue.
 * @param iZwCommandStatusQueueTaskNotificationBitNumber Set by Protocol when a message has been put on the ZW command status queue.
 * @param pProtocolConfig pointer to Protocol Config struct
 * @return true if task was created
 */
bool ZW_ApplicationRegisterTask(VOID_CALLBACKFUNC(appTaskFunc)(SApplicationHandles*),
  uint8_t iZwRxQueueTaskNotificationBitNumber,
  uint8_t iZwCommandStatusQueueTaskNotificationBitNumber,
  const SProtocolConfig_t * pProtocolConfig);

#endif /* _ZW_SYSTEM_STARTUP_H_ */
