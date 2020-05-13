/***************************************************************************//**
 * @brief Weakly defined callbacks for the idle-sleep plugin.
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

//------------------------------------------------------------------------------
// Weak callbacks definitions

WEAK(bool emberAfPluginIdleSleepOkToSleepCallback(uint32_t durationMs))
{
  return true;
}

WEAK(void emberAfPluginIdleSleepWakeUpCallback(uint32_t durationMs))
{
}

WEAK(bool emberAfPluginIdleSleepOkToIdleCallback(void))
{
  return true;
}

WEAK(void emberAfPluginIdleSleepActiveCallback(void))
{
}

WEAK(bool emberAfPluginIdleSleepRtosCallback(uint32_t *durationMs, bool sleepOk))
{
  return false;
}

WEAK(void emberAfPluginIdleSleepPowerModeChangedCallback(uint8_t cpuMode, bool startOrEnd))
{
}
