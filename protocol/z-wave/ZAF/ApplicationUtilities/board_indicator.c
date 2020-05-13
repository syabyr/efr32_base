/**
 * Indicator LED support.
 *
 * @copyright 2018 Silicon Laboratories Inc.
 */

/****************************************************************************/
/*                              INCLUDE FILES                               */
/****************************************************************************/
#include <board.h>
//#define DEBUGPRINT
#include "DebugPrint.h"
#include "Assert.h"
#include "em_letimer.h"
#include "em_cmu.h"
#include <ZAF_PM_Wrapper.h>

/****************************************************************************/
/*                      PRIVATE TYPES and DEFINITIONS                       */
/****************************************************************************/

/**
 * Indicator clock frequency in Hz
 * Must be aligned with clock settings in Board_IndicatorInit().
 */
#define INDICATOR_CLOCK_FREQ 250

/****************************************************************************/
/*                              PRIVATE DATA                                */
/****************************************************************************/

static bool m_indicator_initialized    = false;
static bool m_indicator_active_from_cc = false;

/**
 * Power lock to keep the device from entering deep sleep (EM4) while the
 * indicator is active.
 */
static SPowerLock_t m_IndicatorIoPowerLock;

/**
 * GPIO output value to deactivate the indicator LED
 * Valid values are 0 or 1.
 * Every indicator LED blink period starts by setting the indicator GPIO
 * to this value for off_time_ms (@ref Board_IndicatorControl).
 */
static uint32_t m_indicator_led_off_value = 0;

/****************************************************************************/
/*                      PRIVATE FUNCTIONS                                   */
/****************************************************************************/


/****************************************************************************/
/*                      PUBLIC FUNCTIONS                                    */
/****************************************************************************/

void Board_IndicatorInit(led_id_t led)
{
  if (!m_indicator_initialized)
  {
    /* Configure LED */
    Board_ConfigLed(led, true);

    /* We need a EM3 power lock when the indicator is active.
     * (LETIMER0 will not run in EM4)
     */
    ZAF_PM_Register(&m_IndicatorIoPowerLock, PM_TYPE_PERIPHERAL);

    /* Select the 1000 Hz Ultra Low Frequency RC Oscillator (ULFRCO) as
     * clock source for LFACLK (which is the clock selected by LETIMER0).
     * NB: ULFRCO is always enabled.
     */
    CMU_ClockSelectSet(cmuClock_LFA, cmuSelect_ULFRCO);

    /* The indicator command class uses 1/10 sec increments when configuring
     * the indicator. Ideally we therefore only need a 10 Hz clock to blink
     * the indicator led. But since the prescale can only be 2^n values our
     * best option is to select a prescale of 4 to get a 250 Hz clock. (we
     * could also just skip prescaling and use the 1000 Hz clock as-is, but
     * lower frequency equals lower power usage)
     *
     * NB: Make sure to update INDICATOR_CLOCK_FREQ if making clock changes!
     */
    CMU_ClockPrescSet(cmuClock_LETIMER0, cmuClkDiv_4);

    /* The clock for Low Energy module bus interface and LETIMER0
     * must be enabled
     */
    CMU_ClockEnable(cmuClock_HFLE, true);
    CMU_ClockEnable(cmuClock_LETIMER0, true);

    /* Enable repeat counter 0 interrupt flag */
    LETIMER_IntEnable(LETIMER0, LETIMER_IF_REP0);

    /* Since the LETIMER0 interrupt handler makes a FreeRTOS system call
     * we must set its priority lower (i.e. numerically higher) than
     * configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY. Since a blinking
     * LED is not critical we assign it the lowest possible priority.
     */
    NVIC_SetPriority(LETIMER0_IRQn, configLIBRARY_LOWEST_INTERRUPT_PRIORITY);

    /* Enable LETIMER0 interrupt vector in NVIC*/
    NVIC_EnableIRQ(LETIMER0_IRQn);

    uint32_t letimer_routeloc_led = Board_GetLedLeTimerLoc(led);

    /* Disable output and route LETIMER0 OUT0 to indicator LED location */
    LETIMER0->ROUTEPEN &= ~LETIMER_ROUTEPEN_OUT0PEN;
    LETIMER0->ROUTELOC0 = (LETIMER0->ROUTELOC0 & ~_LETIMER_ROUTELOC0_OUT0LOC_MASK) | letimer_routeloc_led;

    m_indicator_led_off_value = (Board_GetLedOnValue(led) > 0) ? 0 : 1;

    /* Initialize LETIMER0
     * The timer must be initialized (especially the idle values) to avoid
     * getting an ugly looking initial LED output when activated by
     * Board_IndicatorControl()
     */
    const LETIMER_Init_TypeDef letimerInit =
    {
      .enable         = false,                     /* Don't start when init completes. */
      .debugRun       = false,                     /* Stop counter during debug halt. */
      .comp0Top       = false,                     /* Do not load COMP0 into CNT on underflow. */
      .bufTop         = false,                     /* Do not load COMP1 into COMP0 when REP0 reaches 0. */
      .out0Pol        = m_indicator_led_off_value, /* Idle value for output 0. */
      .out1Pol        = m_indicator_led_off_value, /* Idle value for output 1. */
      .ufoa0          = letimerUFOANone,           /* No action on underflow on output 0. */
      .ufoa1          = letimerUFOANone,           /* No action on underflow on output 1. */
      .repMode        = letimerRepeatFree,         /* Count until stopped by SW. */
      .topValue       = 0
    };

    LETIMER_Init(LETIMER0, &letimerInit);

    m_indicator_initialized = true;
  }
}


bool Board_IndicatorControl(uint32_t on_time_ms,
                            uint32_t off_time_ms,
                            uint32_t num_cycles,
                            bool called_from_indicator_cc)
{
  DPRINTF("Board_IndicatorControl() on=%u, off=%u, num=%u\n", on_time_ms, off_time_ms, num_cycles);

  if (!m_indicator_initialized)
  {
    ASSERT(false);
    return false;
  }

  if (on_time_ms < (1000 / INDICATOR_CLOCK_FREQ))
  {
    /* On time is zero (or less than one LETIMER0 clock tick)
     * --> turn off the indicator */
    LETIMER0->ROUTEPEN &= ~LETIMER_ROUTEPEN_OUT0PEN;
    LETIMER_Enable(LETIMER0, false);
    m_indicator_active_from_cc = false;

    ZAF_PM_Cancel(&m_IndicatorIoPowerLock);
  }
  else
  {
    /* LETIMER0 counts down CNT from COMP0 to zero.
     * COMP0 is the period length. COMP0 to COMP1 is the idle time.
     * Output is initially idle and set to m_indicator_led_off_value.
     * When CNT == COMP1 output is set active to inverse value
     * of m_indicator_led_off_value.
     * When CNT == 0 output is set idle again with
     * value m_indicator_led_off_value.
     *
     * Configure @ref m_indicator_led_off_value to a value that will turn
     * off the LED if output to the indicator LED GPIO.
     */
    uint32_t period_length_ticks = ((on_time_ms + off_time_ms) * INDICATOR_CLOCK_FREQ ) / 1000;
    uint32_t off_time_ticks      = (off_time_ms * INDICATOR_CLOCK_FREQ ) / 1000;

    LETIMER_RepeatMode_TypeDef repeat_mode  = letimerRepeatFree;
    uint32_t                   repeat_value = 1;

    if (num_cycles > 0)
    {
      repeat_mode  = letimerRepeatOneshot;
      repeat_value = num_cycles;
    }

    if (period_length_ticks > 0xFFFF || repeat_value > 0xFFFF)
    {
      ASSERT(false);
      return false;
    }

    /* Set COMP0 and COMP1 */
    LETIMER_CompareSet(LETIMER0, 0, period_length_ticks);
    LETIMER_CompareSet(LETIMER0, 1, period_length_ticks - off_time_ticks);

    /* Repetition values REP0 and REP1 must be nonzero for the outputs
     * to switch between idle and active state */
    LETIMER_RepeatSet(LETIMER0, 0, repeat_value);  /* REP0 */
    LETIMER_RepeatSet(LETIMER0, 1, 1);             /* REP1 */

    /* Make sure the timer start fresh */
    LETIMER0->CMD = LETIMER_CMD_CTO0 | LETIMER_CMD_CTO1 | LETIMER_CMD_CLEAR;

    /* Set configurations for LETIMER 0 */
    const LETIMER_Init_TypeDef letimerInit =
    {
      .enable         = false,                     /* Don't start when init completes. */
      .debugRun       = false,                     /* Counter shall not keep running during debug halt. */
      .comp0Top       = true,                      /* Load COMP0 register into CNT when counter underflows. COMP0 is used as TOP */
      .bufTop         = false,                     /* Don't load COMP1 into COMP0 when REP0 reaches 0. */
      .out0Pol        = m_indicator_led_off_value, /* Idle value for output 0. */
      .out1Pol        = m_indicator_led_off_value, /* Idle value for output 1. */
      .ufoa0          = letimerUFOAPwm,            /* PWM output on output 0 */
      .ufoa1          = letimerUFOAPwm,            /* PWM output on output 1*/
      .repMode        = repeat_mode,               /* Repeat count to zero until stopped or REP0 times */
      .topValue       = 0
    };

    /* Enable LETIMER0 OUT0 (location configured in Board_IndicatorInit() */
    LETIMER0->ROUTEPEN |= LETIMER_ROUTEPEN_OUT0PEN;

    /* Initialize LETIMER */
    LETIMER_Init(LETIMER0, &letimerInit);

    /* Set COMP0 and COMP1 */
    LETIMER_CompareSet(LETIMER0, 0, period_length_ticks);
    LETIMER_CompareSet(LETIMER0, 1, period_length_ticks - off_time_ticks);
    LETIMER_Enable(LETIMER0, true);

    m_indicator_active_from_cc = called_from_indicator_cc;

    /* Stay awake until indicator is turned off explicitly with
     * Board_IndicatorControl() or LETIMER0_IRQHandler() is called on
     * blink cycle completion.
     */
    ZAF_PM_StayAwake(&m_IndicatorIoPowerLock, 0);
  }
  return true;
}


bool Board_IsIndicatorActive(void)
{
  DPRINT("Board_IsIndicatorActive()\n");

  return m_indicator_active_from_cc;
}


/**
 * Interrupt handler for LETIMER0.
 *
 * Configured in @ref BoardIndicatorInit to be called whenever the repeat
 * counter 0 of LETIMER0 reaches zero. I.e. whenever the configured number
 * of blink cycles has completed.
 *
 * We don't need to do anything to LETIMER0 here - it will stop automatically
 * when REP0 reaches zero, but we would like to make it possible for the
 * indicator command class to accurately report the status of the indicator.
 * Also we release the power lock to enable deep sleep (EM4).
 *
 */
void LETIMER0_IRQHandler(void)
{
  DPRINT("LETIMER0_IRQHandler()\n");

  if (LETIMER_IntGet(LETIMER0) & LETIMER_IF_REP0)
  {
    DPRINT("LETIMER0_IRQHandler() IF = LETIMER_IF_REP0\n");
    LETIMER_IntClear(LETIMER0, LETIMER_IF_REP0);

    /* Don't know for sure if this is actually needed... */
    LETIMER0->ROUTEPEN &= ~LETIMER_ROUTEPEN_OUT0PEN;
    LETIMER_Enable(LETIMER0, false);

    m_indicator_active_from_cc = false;

    /* We're done blinking and can go back to deep sleep */
    ZAF_PM_CancelFromISR(&m_IndicatorIoPowerLock);
  }
}
