/**
 * @file
 * SWO debug print module
 * @copyright 2019 Silicon Laboratories Inc.
 */


#include "SWO_Debug.h"
#include "hal-config/hal-config.h"
#include "em_gpio.h"
#include <em_dbg.h>
#include <em_cmu.h>
#include <core_cm4.h>
#include "DebugPrintConfig.h"

#define SWO_OUTPUT_FREQ   875000   /*SWO clock frequency in Hz*/

static void ZW_SWO_Print(const uint8_t * pData,uint32_t iLength)
{
  for(int i=0; i < iLength; i++)
  {
    ITM_SendChar(pData[i]);
  }
}

void SWO_DebugPrintInit() {
  static uint8_t buffer[64];
  uint32_t div;
 /*Enable GPIO clock*/
  CMU_ClockEnable(cmuClock_GPIO, true);
 /*Enable SWO output pin SWO PF2*/
  GPIO_DbgSWOEnable(true);
  GPIO_DbgLocationSet(BSP_TRACE_SWO_LOC);
  /* Configure SWO pin for output */
  GPIO_PinModeSet(BSP_TRACE_SWO_PORT, BSP_TRACE_SWO_PIN, gpioModePushPull, 0);
  /* Ensure auxiliary clock going to the Cortex debug trace module is enabled */
  CMU_OscillatorEnable(cmuOsc_AUXHFRCO, true, true);
  /* Enable trace in core debug */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  /* Set TPIU prescaler for the current debug clock frequency. Target frequency
     is swo_freq Hz so we choose a divider that gives us the closest match.
     Actual divider is TPI->ACPR + 1. */
  div  = (CMU_ClockFreqGet(cmuClock_DBG) + (SWO_OUTPUT_FREQ / 2)) / SWO_OUTPUT_FREQ;

  TPI->SPPR = 2; /* Set protocol to NRZ */
  TPI->ACPR = div - 1;   /* "Async Clock Prescaler Register". Scale the baud rate of the asynchronous output */
  /* Unlock ITM and output data */
  ITM->LAR  = 0xC5ACCE55;
  ITM->TCR  = ITM_TCR_TraceBusID_Msk | ITM_TCR_SWOENA_Msk | ITM_TCR_SYNCENA_Msk | ITM_TCR_ITMENA_Msk; /* ITM Trace Control Register */
  ITM->TPR  = ITM_TPR_PRIVMASK_Msk; /* ITM Trace Privilege Register make channel 0 accessible by user code*/
  /* ITM Channel 0 is used for UART output */
  ITM->TER  = 0x1; /* ITM Trace Enable Register. Enabled tracing on stimulus ports. One bit per stimulus port. */
  DWT->CTRL = 0x400003FE;
  /* Disable continuous formatting */
  TPI->FFCR = 0x00000100; /* Formatter and Flush Control Register */
  DebugPrintConfig(buffer,sizeof(buffer) ,ZW_SWO_Print);
}
