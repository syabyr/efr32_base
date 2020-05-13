/***************************************************************************//**
 * @brief Weakly defined Micrium RTOS callbacks.
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

//------------------------------------------------------------------------------
// Weak callbacks definitions

WEAK(bool emberAfPluginMicriumRtosOkToEnterLowPowerCallback(bool isEm2,
                                                            uint32_t durationMs))
{
  return true;
}

WEAK(void emberAfPluginMicriumRtosAppTask1InitCallback(void))
{
}

WEAK(void emberAfPluginMicriumRtosAppTask1MainLoopCallback(void *p_arg))
{
  while (true) {
    // Main loop
  }
}

WEAK(void emberAfPluginMicriumRtosAppTask2InitCallback(void))
{
}

WEAK(void emberAfPluginMicriumRtosAppTask2MainLoopCallback(void *p_arg))
{
  while (true) {
    // Main loop
  }
}

WEAK(void emberAfPluginMicriumRtosAppTask3InitCallback(void))
{
}

WEAK(void emberAfPluginMicriumRtosAppTask3MainLoopCallback(void *p_arg))
{
  while (true) {
    // Main loop
  }
}
