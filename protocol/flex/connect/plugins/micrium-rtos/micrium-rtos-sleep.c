/***************************************************************************//**
 * @brief MIcrium RTOS sleep code.
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
#include "hal.h"
#include "sleep.h"
#include "rail.h"
#include "sl_status.h"

#include "micrium-rtos-support.h"
#include "flex-callbacks.h"

//------------------------------------------------------------------------------
// Static variables.

static bool gpioControlSleepAllowed = true;
static sl_sleeptimer_timer_handle_t wakeupTimerId;

//------------------------------------------------------------------------------
// Forward declarations.

static uint16_t EM2WakeupProcessTime(void);

//------------------------------------------------------------------------------
// Sleep handler - invoked from the idle task hook.

void emAfPluginMicriumRtosSleepHandler(void)
{
  uint32_t sleepTimeMs, stackTaskNextWakeUpTimeMs, afTaskNextWakeUpTimeMs,
           callTimeMs;
  bool systemCanSleep = false;

  INTERRUPTS_OFF();

  callTimeMs = halCommonGetInt32uMillisecondTick();
  stackTaskNextWakeUpTimeMs =
    emAfPluginMicriumRtosStackTaskGetNextWakeUpTimeMs();
  afTaskNextWakeUpTimeMs =
    emAfPluginMicriumRtosAfTaskGetNextWakeUpTimeMs();

  if (timeGTorEqualInt32u(callTimeMs, stackTaskNextWakeUpTimeMs)
      || timeGTorEqualInt32u(callTimeMs, afTaskNextWakeUpTimeMs)) {
    INTERRUPTS_ON();
    return;
  } else {
    sleepTimeMs = elapsedTimeInt32u(callTimeMs, stackTaskNextWakeUpTimeMs);
    if (sleepTimeMs > elapsedTimeInt32u(callTimeMs, afTaskNextWakeUpTimeMs)) {
      sleepTimeMs = elapsedTimeInt32u(callTimeMs, afTaskNextWakeUpTimeMs);
    }
  }

  if (gpioControlSleepAllowed
#if defined(EMBER_AF_PLUGIN_BLE)
      // The BLE stacks keeps the SLEEP manager updated with the current lowest
      // energy mode allowed, so we just query the SLEEP manager to determine
      // whether BLE can deep sleep or not.
      && SLEEP_LowestEnergyModeGet() != sleepEM1
#endif // EMBER_AF_PLUGIN_BLE
      && emAfPluginMicriumRtosStackTaskIsDeepSleepAllowed()
      && emAfPluginMicriumRtosAfTaskIsDeepSleepAllowed()
      && emberAfPluginMicriumRtosOkToEnterLowPowerCallback(true, sleepTimeMs)) {
    RAIL_Status_t status = RAIL_Sleep(EM2WakeupProcessTime(), &systemCanSleep);

    // RAIL_Sleep() returns a non-success status when the radio is not idle. We
    // do not deep sleep in that case.
    if (status != RAIL_STATUS_NO_ERROR) {
      systemCanSleep = false;
    }
  }

  if (systemCanSleep) {
    RTOS_ERR err;

    // Lock the OS scheduler so that we can get to the RAIL_Wake() call once we
    // are done deep sleeping.
    OSSchedLock(&err);
    assert(RTOS_ERR_CODE_GET(err) == RTOS_ERR_NONE);

    halPowerDown();

    // Start a timer to get us out of EM2.
    assert(sl_sleeptimer_start_timer_ms(&wakeupTimerId,
                                        sleepTimeMs,
                                        NULL,
                                        NULL,
                                        0u,
                                        0u) == SL_STATUS_OK);

    // Enter EM2.
    halSleepPreserveInts(SLEEPMODE_WAKETIMER);

    assert(RAIL_Wake(0) == RAIL_STATUS_NO_ERROR);

    INTERRUPTS_ON();

    halPowerUp();

    sl_sleeptimer_stop_timer(&wakeupTimerId);

    OSSchedUnlock(&err);
    assert(RTOS_ERR_CODE_GET(err) == RTOS_ERR_NONE);
  } else if (emberAfPluginMicriumRtosOkToEnterLowPowerCallback(false,
                                                               sleepTimeMs)) {
    halCommonIdleForMilliseconds(&sleepTimeMs);
  } else {
    INTERRUPTS_ON();
  }
}

#ifdef EMBER_AF_PLUGIN_MICRIUM_RTOS_USE_BUTTONS
void halButtonIsr(uint8_t button, uint8_t state)
{
  if (state == BUTTON_PRESSED) {
    gpioControlSleepAllowed = (button == BUTTON1);
  }
}
#endif

//------------------------------------------------------------------------------
// Static functions.

// Time required by the hardware to come out of EM2 in microseconds.
// This value includes HW startup, emlib and sleepdrv execution time.
// Voltage scaling, HFXO startup and HFXO steady times are excluded from
// this because they are handled separately. RTCCSYNC time is also
// excluded and it is handled by RTCCSYNC code itself.
#define EM2_WAKEUP_PROCESS_TIME_OVERHEAD_US (390)

// Time it takes to upscale voltage after EM2 in microseconds.
#define EM2_WAKEUP_VSCALE_OVERHEAD_US (30)

/* one cycle is 83 ns */
#define TIMEOUT_PERIOD_US(cycles) (cycles * 83 / 1000)
static const uint16_t timeoutPeriodTable[] =
{
  TIMEOUT_PERIOD_US(2),       /* 0 = 2 cycles */
  TIMEOUT_PERIOD_US(4),       /* 1 = 4 cycles */
  TIMEOUT_PERIOD_US(16),      /* 2 = 16 cycles */
  TIMEOUT_PERIOD_US(32),      /* 3 = 32 cycles */
  TIMEOUT_PERIOD_US(256),     /* 4 = 256 cycles */
  TIMEOUT_PERIOD_US(1024),    /* 5 = 1024 cycles */
  TIMEOUT_PERIOD_US(2048),    /* 6 = 2048 cycles */
  TIMEOUT_PERIOD_US(4096),    /* 7 = 4096 cycles */
  TIMEOUT_PERIOD_US(8192),    /* 8 = 8192 cycles */
  TIMEOUT_PERIOD_US(16384),   /* 9 = 16384 cycles */
  TIMEOUT_PERIOD_US(32768),   /* 10 = 32768 cycles */
};

static bool isHfxoAutoSelectEnabled(void)
{
#ifdef _CMU_HFXOCTRL_AUTOSTARTSELEM0EM1_MASK
  if (CMU->HFXOCTRL & _CMU_HFXOCTRL_AUTOSTARTSELEM0EM1_MASK) {
    return true;
  }
#endif
  return false;
}

static bool isEm23VScaleEnabled(void)
{
#ifdef _EMU_CTRL_EM23VSCALE_MASK
  if (EMU->CTRL & _EMU_CTRL_EM23VSCALE_MASK) {
    return true;
  }
#endif
  return false;
}

static uint16_t em2WakeupVScaleOverhead(void)
{
  if (!isEm23VScaleEnabled()) {
    return 0;
  }

  if (!isHfxoAutoSelectEnabled()) {
    // If HFXO auto select is disabled, the voltage upscaling is done in
    // EMLIB while waiting for HFXO to stabilize, thus adding no overhead
    // to the overall wakeup time.
    return 0;
  }

  return EM2_WAKEUP_VSCALE_OVERHEAD_US;
}

static uint16_t EM2WakeupProcessTime(void)
{
#ifndef _SILICON_LABS_32B_SERIES_2
  uint8_t steady = ((CMU->HFXOTIMEOUTCTRL
                     & _CMU_HFXOTIMEOUTCTRL_STEADYTIMEOUT_MASK)
                    >> _CMU_HFXOTIMEOUTCTRL_STEADYTIMEOUT_SHIFT);
  uint8_t startup = ((CMU->HFXOTIMEOUTCTRL
                      & _CMU_HFXOTIMEOUTCTRL_STARTUPTIMEOUT_MASK)
                     >> _CMU_HFXOTIMEOUTCTRL_STARTUPTIMEOUT_SHIFT);
  return timeoutPeriodTable[steady] + timeoutPeriodTable[startup]
         + EM2_WAKEUP_PROCESS_TIME_OVERHEAD_US + em2WakeupVScaleOverhead();
#else
  // Note: (EMHAL-1521) return some safe value (1ms) until we have all the
  // proper registers for EFR series 2.
  return 1000;
#endif
}
