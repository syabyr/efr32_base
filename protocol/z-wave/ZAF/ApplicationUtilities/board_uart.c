/***************************************************************************
* @file board_uart.c
* @copyright 2018 Silicon Laboratories Inc.
* @brief Provides support for UART functionality
*
* @author Thomas Roll
* @date 2014/12/15
*/

/****************************************************************************/
/*                              INCLUDE FILES                               */
/****************************************************************************/
#include <board.h>
#include <CommandHandler.h>
#include <ZW_uart_api.h>
//#define DEBUGPRINT
#include "DebugPrint.h"
#include "ApplicationResetHandler.h"
#include <ZW_application_transport_interface.h>

#include "ev_man.h"
#include "zaf_event_helper.h"

/****************************************************************************/
/*                      PRIVATE TYPES and DEFINITIONS                       */
/****************************************************************************/

/****************************************************************************/
/*                              PRIVATE DATA                                */
/****************************************************************************/

#define EVENT_DISABLED 0
#define EVENT_ENABLED  1

uint8_t mBoardEventSettings[BOARD_EVENT_SIZE] = {0, };

const char * mStatusMessage[] = {"Board Idle\r\n",               // BOARD_STATUS_IDLE
                                 "Power Down\r\n",               // BOARD_STATUS_POWER_DOWN
                                 "Learn mode active\r\n",        // BOARD_STATUS_LEARNMODE_ACTIVE
                                 "Learn mode inactive\r\n"       // BOARD_STATUS_LEARNMODE_INACTIVE
                                 };


/****************************************************************************/
/*                              EXPORTED DATA                               */
/****************************************************************************/
extern SApplicationHandles* g_pAppHandles;

/****************************************************************************/
/*                            PRIVATE FUNCTIONS                             */
/****************************************************************************/

static void EnqueueEvent(const uint8_t event)
{
  ZAF_EventHelperEventEnqueue(event);
}

static void BoardEventHandlerLearnMode(void)
{
  EnqueueEvent(EVENT_SYSTEM_LEARNMODE_START);
}

static void BoardEventHandlerPirEvent(void)
{
  EnqueueEvent(EVENT_KEY_B2_PRESS);
}

static void BoardEventHandlerKeyB0Press(void)
{
  EnqueueEvent(EVENT_KEY_B0_PRESS);
}

static void BoardEventHandlerKeyB0Held(void)
{
  EnqueueEvent(EVENT_KEY_B0_HELD_5_SEC);
}

static void BoardEventHandlerKeyB0Up(void)
{
  EnqueueEvent(EVENT_KEY_B0_UP);
}

static void BoardEventHandlerKeyB1Press(void)
{
  EnqueueEvent(EVENT_KEY_B1_PRESS);
}

static void BoardEventHandlerKeyB1Held(void)
{
  EnqueueEvent(EVENT_KEY_B1_HELD_5_SEC);
}

static void BoardEventHandlerKeyB1Up(void)
{
  EnqueueEvent(EVENT_KEY_B1_UP);
}

static void BoardEventHandlerKeyB2Press(void)
{
  EnqueueEvent(EVENT_KEY_B2_PRESS);
}

static void BoardEventHandlerKeyB2Held(void)
{
  EnqueueEvent(EVENT_KEY_B2_HELD);
}

static void BoardEventHandlerKeyB2Up(void)
{
  EnqueueEvent(EVENT_KEY_B2_UP);
}

static void BoardEventHandlerKeyB3Press(void)
{
  EnqueueEvent(EVENT_KEY_B3_PRESS);
}

static void BoardEventHandlerKeyB3Held(void)
{
  EnqueueEvent(EVENT_KEY_B3_HELD);
}

static void BoardEventHandlerKeyB3Up(void)
{
  EnqueueEvent(EVENT_KEY_B3_UP);
}

static void BoardEventHandlerKeyB4Press(void)
{
  EnqueueEvent(EVENT_KEY_B4_PRESS);
}

static void BoardEventHandlerKeyB4Held(void)
{
  EnqueueEvent(EVENT_KEY_B4_HELD);
}

static void BoardEventHandlerKeyB4Up(void)
{
  EnqueueEvent(EVENT_KEY_B4_UP);
}

static void BoardEventHandlerKeyB6Press(void)
{
  EnqueueEvent(EVENT_KEY_B6_PRESS);
}

static void BoardEventHandlerKeyB6Held(void)
{
  EnqueueEvent(EVENT_KEY_B6_HELD);
}

static void BoardEventHandlerKeyB6Up(void)
{
  EnqueueEvent(EVENT_KEY_B6_UP);
}

static void BoardEventHandlerOpenDoorLatchEvent(void)
{
  EnqueueEvent(EVENT_KEY_B1_DOWN);
}

static void BoardEventHandlerCloseDoorLatchEvent(void)
{
  EnqueueEvent(EVENT_KEY_B1_UP);
}

static void BoardEventHandlerKeypadEvent(void)
{
  EnqueueEvent(EVENT_WAKEUP_EXT_INT);
}

void BoardResetHandler(void)
{
  ApplicationReset(0x0, 0x0);
}

static void BoardClearFileSystemHandler(void)
{
  /* Reset protocol */
  // Set default command to protocol
  SZwaveCommandPackage CommandPackage;
  CommandPackage.eCommandType = EZWAVECOMMANDTYPE_SET_DEFAULT;
  EQueueNotifyingStatus Status = QueueNotifyingSendToBack(g_pAppHandles->pZwCommandQueue, (uint8_t*)&CommandPackage, 500);
  if (EQUEUENOTIFYING_STATUS_SUCCESS != Status)
  {
    ZW_UART0_tx_send_str((uint8_t *)"Failed protocol reset\r\n");
  }
}

/*============================ InitZDP03A ===============================
**-------------------------------------------------------------------------*/
uint32_t BoardInit(void)
{
  DPRINT("InitUart");

  CommandHandlerInit();
  CommandHandlerAddCommand('r', "Reset button", &BoardResetHandler);
  CommandHandlerAddCommand('c', "Clear File System", &BoardClearFileSystemHandler);

  return 0;
}

void BoardInitPWMtimer(uint32_t maxCompareLevel)
{
  ;
}

void BoardEnablePWM(LED_OUT led)
{
  ;
}

void BoardInitLedBlinkTimer(uint32_t maxCompareLevel)
{
  ;
}

void BoardEnableEvent(BOARD_EVENT event)
{
  // On a physical board, this would set up pin as input and pull up/pull down accordingly.
  // The event <-> pin map will determine which pin and action (single press/hold/release/multi press) is used for which event.
  mBoardEventSettings[event] = EVENT_ENABLED;

  if (BOARD_EVENT_LEARNMODE_START == event)
  {
    CommandHandlerAddCommand('l', "Learn Mode start", &BoardEventHandlerLearnMode);
  }

  if (BOARD_EVENT_PIR_EVENT == event)
  {
    CommandHandlerAddCommand('p', "PIR Event", &BoardEventHandlerPirEvent);
  }

  if (BOARD_EVENT_KEY_B0_EVENTS == event)
  {
    CommandHandlerAddCommand('0', "KEY_B0 Press",       &BoardEventHandlerKeyB0Press);
    CommandHandlerAddCommand('!', "KEY_B0 Held 10 sec", &BoardEventHandlerKeyB0Held);
    CommandHandlerAddCommand('Q', "KEY_B0 Up\r\n",      &BoardEventHandlerKeyB0Up);
  }

  if (BOARD_EVENT_KEY_B1_EVENTS == event)
  {
    CommandHandlerAddCommand('1', "KEY_B1 Press",       &BoardEventHandlerKeyB1Press);
    CommandHandlerAddCommand('!', "KEY_B1 Held 10 sec", &BoardEventHandlerKeyB1Held);
    CommandHandlerAddCommand('Q', "KEY_B1 Up\r\n",      &BoardEventHandlerKeyB1Up);
  }

  if (BOARD_EVENT_KEY_B2_EVENTS == event)
  {
    CommandHandlerAddCommand('2', "KEY_B2 Press",  &BoardEventHandlerKeyB2Press);
    CommandHandlerAddCommand('@', "KEY_B2 Held",   &BoardEventHandlerKeyB2Held);
    CommandHandlerAddCommand('W', "KEY_B2 Up\r\n", &BoardEventHandlerKeyB2Up);
  }

  if (BOARD_EVENT_KEY_B3_EVENTS == event)
  {
    CommandHandlerAddCommand('3', "KEY_B3 Press",  &BoardEventHandlerKeyB3Press);
    CommandHandlerAddCommand('#', "KEY_B3 Held",   &BoardEventHandlerKeyB3Held);
    CommandHandlerAddCommand('E', "KEY_B3 Up\r\n", &BoardEventHandlerKeyB3Up);
  }

  if (BOARD_EVENT_KEY_B4_EVENTS == event)
  {
    CommandHandlerAddCommand('4', "KEY_B4 Press",  &BoardEventHandlerKeyB4Press);
    CommandHandlerAddCommand('$', "KEY_B4 Held",   &BoardEventHandlerKeyB4Held);
    CommandHandlerAddCommand('R', "KEY_B4 Up\r\n", &BoardEventHandlerKeyB4Up);
  }

  if (BOARD_EVENT_KEY_B6_EVENTS == event)
  {
    CommandHandlerAddCommand('6', "KEY_B6 Press",  &BoardEventHandlerKeyB6Press);
    CommandHandlerAddCommand('^', "KEY_B6 Held",   &BoardEventHandlerKeyB6Held);
    CommandHandlerAddCommand('Y', "KEY_B6 Up\r\n", &BoardEventHandlerKeyB6Up);
  }

  if (BOARD_EVENT_KEYPAD_START == event)
  {
    CommandHandlerAddCommand('k', "Keypad Start", &BoardEventHandlerKeypadEvent);
  }

  if (BOARD_EVENT_DOORLATCH_START == event)
  {
    CommandHandlerAddCommand('d', "Open Door Latch", &BoardEventHandlerOpenDoorLatchEvent);
    CommandHandlerAddCommand('u', "Close Door Latch", &BoardEventHandlerCloseDoorLatchEvent);
  }
}

void BoardDisableEvent(BOARD_EVENT event)
{
  // On a physical board, this would set up pin as input and pull up/pull down accordingly.
  // The event <-> pin map will determine which pin and action (single press/hold/release/multi press) is used for which event.
  mBoardEventSettings[event] = EVENT_DISABLED;
}

void BoardIndicateStatus(BOARD_STATUS status)
{
  if(BOARD_STATUS_IDLE == status)
  {
    Led(LED_D1, OFF);
  }
  else if(BOARD_STATUS_LEARNMODE_ACTIVE == status)
  {
    Led(LED_D1, ON);
  }
  else if(BOARD_STATUS_LEARNMODE_INACTIVE == status)
  {
    Led(LED_D1, OFF);
  }
}


void ConfigKeyInput( KEY_IN key, bool pullUp)
{
  ;
}

void ConfigLedOutput(LED_OUT led, bool enable, uint32_t dout)
{
  ;
}


void Led( LED_OUT led, LED_ACTION action)
{
  if(ON == action){
    DPRINTF("\r\nLED_D%u ON\r\n", (uint32_t)led);
  }
  else
  {
    DPRINTF("\r\nLED_D%u OFF\r\n", (uint32_t)led);
  }
}

void setLedPWM(LED_OUT led, uint8_t pwm)
{
  ;
}

uint32_t KeyGet(KEY_IN key)
{
  return 0;
}

void board_GPIO_PinModeSet(GPIO_Port_TypeDef port,
                           unsigned int pin,
                           GPIO_Mode_TypeDef mode,
                           unsigned int out)
{
  ;
}

void board_GPIO_PinOutClear(GPIO_Port_TypeDef port, unsigned int pin)
{
  ;
}

void board_GPIO_PinOutSet(GPIO_Port_TypeDef port, unsigned int pin)
{
  ;
}


void Board_IndicatorInit(led_id_t led,
                         uint32_t letimer_out0_routeloc,
                         uint32_t led_off_value)
{
  ;
}


bool Board_IndicatorControl(uint32_t on_time_ms,
                            uint32_t off_time_ms,
                            uint32_t num_cycles,
                            bool called_from_indicator_cc)
{
  return true;
}


bool Board_IsIndicatorActive(void)
{
  return false;
}
