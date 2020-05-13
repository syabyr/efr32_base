/***************************************************************************//**
 * @file
 * @brief Range Test Software Example.
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

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rail.h"
#include "rail_types.h"
#include "rail_chip_specific.h"
#include "pa_conversions_efr32.h"
#include "pa_curves_efr32.h"

#include "spidrv.h"
#include "em_device.h"
#include "em_chip.h"
#include "em_cmu.h"
#include "em_emu.h"
#include "em_gpio.h"
#include "em_core.h"
#include "bsp.h"
#include "retargetserialhalconfig.h"
#include "gpiointerrupt.h"

#include "app_common.h"

#include "pushButton.h"

#include "uartdrv.h"

#include "hal_common.h"

#include "sl_sleeptimer.h"
#include "bsp.h"
#include "graphics.h"
#include "menu.h"
#include "seq.h"

#include "rangeTest.h"
#include "rail_ieee802154.h"
#include "rail_ble.h"

//Tx buffer for the UART logging
DEFINE_BUF_QUEUE(64u, UartTxQueue);

// ----------------------------------------------------------------------------
// Function Prototypes
void RAILCb_Generic(RAIL_Handle_t railHandle, RAIL_Events_t events);
void rangeTestMASet(uint32_t nr);
void rangeTestMAClear();
void rangeTestMAClearAll();
static void registerEvents(railEvents_t* railEvents, RAIL_Events_t events);
static RAIL_Status_t initBLEPHY();
static RAIL_Status_t initIEEE802154();
static void periodicTimerCallback(sl_sleeptimer_timer_handle_t *handle, void *data);
static rangeTestPacket_t* getStartOfPayload(uint8_t* buf);

// Variables that is used to exchange data between the event events and
// scheduled routines
volatile bool pktRx, pktLog, txReady, txScheduled;
// setting and states of the Range Test
volatile rangeTest_t rangeTest;

static sl_sleeptimer_timer_handle_t periodicTimer;

// Memory allocation for RAIL
__ALIGNED(4) // EFR32XG22 needs 32-bit alignment
static uint8_t railTxFifo[RAIL_TX_BUFFER_SIZE] = { 0 };
// Buffer where Tx data stored
static uint8_t txBuffer[RAIL_TX_BUFFER_SIZE] = { 0 };
// Buffer where Rx data stored
static uint8_t rxBuffer[RAIL_RX_BUFFER_SIZE] = { 0 };
// The struct is used for registering the rail events. Can be used during development and debugging
static railEvents_t railEvents = { 0 };

const RAIL_IEEE802154_Config_t rail154Config = {
  .addresses = NULL,
  .ackConfig = {
    .enable = false,      // Turn on auto ACK for IEEE 802.15.4.
    .ackTimeout = 672,    // 54-12 symbols * 16 us/symbol = 672 us.
    .rxTransitions = {
      .success = RAIL_RF_STATE_RX,    // Go to TX to send the ACK.
      .error = RAIL_RF_STATE_RX,      // For an always-on device stay in RX.
    },
    .txTransitions = {
      .success = RAIL_RF_STATE_RX,    // Go to RX for receiving the ACK.
      .error = RAIL_RF_STATE_RX,      // For an always-on device stay in RX.
    },
  },
  .timings = {
    .idleToRx = 100,
    .idleToTx = 100,
    .rxToTx = 192,      // 12 symbols * 16 us/symbol = 192 us
    .txToRx = 192,      // 12 symbols * 16 us/symbol = 192 us
    .rxSearchTimeout = 0,   // Not used
    .txToRxSearchTimeout = 0,   // Not used
  },
  .framesMask = RAIL_IEEE802154_ACCEPT_STANDARD_FRAMES,
  .promiscuousMode = true,    // Enable format and address filtering.
  .isPanCoordinator = false,
};
static uint16_t physicalCh = 0;
static uint16_t logicalCh = 0;

RAIL_Handle_t railHandle = NULL;

static RAIL_Config_t railCfg = {
  .eventsCallback = &RAILCb_Generic,
};

// UART logging handles
UARTDRV_HandleData_t  UARTHandleData;
UARTDRV_Handle_t      UARTHandle = &UARTHandleData;

void (*repeatCallbackFn)(void) = NULL;
uint32_t repeatCallbackTimeout;

/*****************************************************************************
* @brief     Initialize GPIOs direction and default state for VCP.
*
* @param     None.
*
* @return    None.
*****************************************************************************/
static void halInternalInitVCPPins()
{
  /* Configure GPIO pin for UART TX */
  /* To avoid false start, configure output as high. */
  GPIO_PinModeSet(RETARGET_TXPORT, RETARGET_TXPIN, gpioModePushPull, 1u);
  /* Configure GPIO pin for UART RX */
  GPIO_PinModeSet(RETARGET_RXPORT, RETARGET_RXPIN, gpioModeInput, 1u);
  /* Enable the switch that enables UART communication. */
  RETARGET_PERIPHERAL_ENABLE();
#if defined(_USART_ROUTELOC0_MASK)
  RETARGET_UART->ROUTELOC0 |= USART_ROUTEPEN_RXPEN | USART_ROUTEPEN_TXPEN;
#endif
}

/**************************************************************************//**
 * @brief Setup GPIO interrupt for pushbuttons.
 *****************************************************************************/
static void GpioSetup(void)
{
  // Enable GPIO clock
  CMU_ClockEnable(cmuClock_GPIO, true);

  // Enable the buttons on the board
  for (int i = 0; i < BSP_NO_OF_BUTTONS; i++) {
    GPIO_PinModeSet(buttonArray[i].port, buttonArray[i].pin, gpioModeInputPull, 1);
  }

  // Initialize GPIOs direction and default state for VCP
  halInternalInitVCPPins();
}

/**************************************************************************//**
 * @brief   Register a callback function to be called repeatedly at the
 *          specified frequency.
 *
 * @param   pFunction: Pointer to function that should be called at the
 *                     given frequency.
 * @param   pParameter: Pointer argument to be passed to the callback function.
 * @param   frequency: Frequency at which to call function at.
 *
 * @return  0 for successful or
 *         -1 if the requested frequency is not supported.
 *****************************************************************************/
int RepeatCallbackRegister(void(*pFunction)(void*),
                           void* pParameter,
                           unsigned int timeout)
{
  repeatCallbackFn = (void(*)(void))pFunction;
  repeatCallbackTimeout = timeout;

  return 0;
}

// ----------------------------------------------------------------------------
// RAIL Callbacks

/**************************************************************************//**
 * @brief      Interrupt level callback for RAIL events.
 *
 * @return     None
 *****************************************************************************/
void  RAILCb_Generic(RAIL_Handle_t railHandle, RAIL_Events_t events)
{
  if (events & RAIL_EVENT_TX_PACKET_SENT) {
    txReady = true;
    txScheduled = false;

    if (rangeTest.log) {
      pktLog = true;
    }
  }

  if (events & RAIL_EVENT_RX_FRAME_ERROR) {
    // CRC error callback enabled
    // Put radio back to RX (RAIL doesn't support it as of now..)
    RAIL_StartRx(railHandle, physicalCh, NULL);

    // Count CRC errors
    if (rangeTest.pktsCRC < 0xFFFF) {
      rangeTest.pktsCRC++;
    }
  }

  if (events & RAIL_EVENT_RX_PACKET_RECEIVED) {
    RAIL_RxPacketInfo_t packetInfo;
    RAIL_RxPacketDetails_t packetDetails;
    RAIL_RxPacketHandle_t packetHandle =
      RAIL_GetRxPacketInfo(railHandle,
                           RAIL_RX_PACKET_HANDLE_NEWEST,
                           &packetInfo);

    if ((packetInfo.packetStatus != RAIL_RX_PACKET_READY_SUCCESS)
        && (packetInfo.packetStatus != RAIL_RX_PACKET_READY_CRC_ERROR)) {
      // RAIL_EVENT_RX_PACKET_RECEIVED must be handled last in order to return
      // early on aborted packets here.
      return;
    }

    RAIL_GetRxPacketDetails(railHandle, packetHandle, &packetDetails);

    uint16_t length = packetInfo.packetBytes;

    // Read packet data into our packet structure
    memcpy(rxBuffer,
           packetInfo.firstPortionData,
           packetInfo.firstPortionBytes);
    memcpy(rxBuffer + packetInfo.firstPortionBytes,
           packetInfo.lastPortionData,
           length - packetInfo.firstPortionBytes);

    static uint32_t lastPktCnt = 0u;
    int8_t RssiValue = 0;

    rangeTestPacket_t * pRxPacket = getStartOfPayload(rxBuffer);

    if (rangeTest.isRunning) {
      // Buffer variables for  volatile fields
      uint16_t pktsCnt;
      uint16_t pktsRcvd;

      // Put radio back to RX (RAIL doesn't support it as of now..)
      RAIL_StartRx(railHandle, physicalCh, NULL);

      // Make sure the packet addressed to me
      if (pRxPacket->destID != rangeTest.srcID) {
        return;
      }

      // Make sure the packet sent by the selected remote
      if (pRxPacket->srcID != rangeTest.destID) {
        return;
      }

      if ( (RANGETEST_PACKET_COUNT_INVALID == rangeTest.pktsRcvd)
           || (pRxPacket->pktCounter <= rangeTest.pktsCnt) ) {
        // First packet received OR
        // Received packet counter lower than already received counter.

        // Reset received counter
        rangeTest.pktsRcvd = 0u;
        // Set counter offset
        rangeTest.pktsOffset = pRxPacket->pktCounter - 1u;

        // Clear RSSI Chart
        GRAPHICS_RSSIClear();

        // Clear Moving-Average history
        rangeTestMAClearAll();

        // Restart Moving-Average calculation
        lastPktCnt = 0u;
      }

      if (rangeTest.pktsRcvd < 0xFFFF) {
        rangeTest.pktsRcvd++;
      }

      rangeTest.pktsCnt = pRxPacket->pktCounter - rangeTest.pktsOffset;
      rangeTest.rssiLatch = packetDetails.rssi;

      // Calculate recently lost packets number based on newest counter
      if ((rangeTest.pktsCnt - lastPktCnt) > 1u) {
        // At least one packet lost
        rangeTestMASet(rangeTest.pktsCnt - lastPktCnt - 1u);
      }
      // Current packet is received
      rangeTestMAClear();

      lastPktCnt = rangeTest.pktsCnt;

      // Store RSSI value from the latch
      RssiValue = (int8_t)(rangeTest.rssiLatch);
      // Limit stored RSSI values to the displayable range
      RssiValue = (RssiChartAxis[GRAPHICS_RSSI_MIN_INDEX] > RssiValue)
                  // If lower than minimum -> minimum
                  ? (RssiChartAxis[GRAPHICS_RSSI_MIN_INDEX])
                  // else check if higher than maximum
                  : ((RssiChartAxis[GRAPHICS_RSSI_MAX_INDEX] < RssiValue)
                     // Higher than maximum -> maximum
                     ? (RssiChartAxis[GRAPHICS_RSSI_MAX_INDEX])
                     // else value is OK
                     : (RssiValue));

      // Store RSSI value in ring buffer
      GRAPHICS_RSSIAdd(RssiValue);

      // Calculate Error Rates here to get numbers to print in case log is enabled
      // These calculation shouldn't take too long.

      // Calculate Moving-Average Error Rate
      rangeTest.MA =  (rangeTestMAGet() * 100.0f) / rangeTest.maSize;

      // Buffering volatile values
      pktsCnt = rangeTest.pktsCnt;
      pktsRcvd = rangeTest.pktsRcvd;

      // Calculate Packet Error Rate
      rangeTest.PER = (pktsCnt) // Avoid zero division
                      ? (((float)(pktsCnt - pktsRcvd) * 100.0f)
                         / pktsCnt) // Calculate PER
                      : 0.0f;     // By default PER is 0.0%
      if (rangeTest.log) {
        pktLog = true;
      }
      pktRx = true;
    }
  }

  registerEvents(&railEvents, events);
}

/**************************************************************************//**
 * @brief  Function to fill remainder of the packet to be sent with 0x55, 0xAA.
 *
 * @param  remainder: TX buffer to fill out with the gene.
 *
 * @return None.
 *****************************************************************************/
void rangeTestGenerateRemainder(uint8_t *remainder)
{
  uint8_t remainderLength = rangeTest.payloadLength - PAYLOAD_LEN_MIN;
  for (int i = 0; i < remainderLength; i++) {
    remainder[i] = (i % 2u) ? (0x55u) : (0xAAu);
  }
}

/**************************************************************************//**
 * @brief  Function to count how many bits has the value of 1.
 *
 * @param  u: Input value to count its '1' bits.
 *
 * @return Number of '1' bits in the input.
 *****************************************************************************/
uint32_t rangeTestCountBits(uint32_t u)
{
  uint32_t uCount = u
                    - ((u >> 1u) & 033333333333)
                    - ((u >> 2u) & 011111111111);

  return  (((uCount + (uCount >> 3u)) & 030707070707) % 63u);
}

/**************************************************************************//**
 * @brief  This function inserts a number of bits into the moving average
 *         history.
 *
 * @param  nr: The value to be inserted into the history.
 *
 * @return None.
 *****************************************************************************/
void rangeTestMASet(uint32_t nr)
{
  uint8_t i;
  // Buffering volatile fields
  uint8_t  maFinger = rangeTest.maFinger;

  if (nr >= rangeTest.maSize) {
    // Set all bits to 1's
    i = rangeTest.maSize;

    while (i >> 5u) {
      rangeTest.maHistory[(i >> 5u) - 1u] = 0xFFFFFFFFul;
      i -= 32u;
    }
    return;
  }

  while (nr) {
    rangeTest.maHistory[maFinger >> 5u] |= (1u << maFinger % 32u);
    maFinger++;
    if (maFinger >= rangeTest.maSize) {
      maFinger = 0u;
    }
    nr--;
  }
  // Update the bufferd value back to the volatile field
  rangeTest.maFinger = maFinger;
}

/**************************************************************************//**
 * @brief  This function clears the most recent bit in the moving average
 *         history. This indicates that last time we did not see any missing
 *         packages.
 *
 * @param  nr: The value to be inserted into the history.
 *
 * @return None.
 *****************************************************************************/
void rangeTestMAClear()
{
  // Buffering volatile value
  uint8_t  maFinger = rangeTest.maFinger;

  rangeTest.maHistory[maFinger >> 5u] &= ~(1u << (maFinger % 32u));

  maFinger++;
  if (maFinger >= rangeTest.maSize) {
    maFinger = 0u;
  }
  // Updating new value back to volatile
  rangeTest.maFinger = maFinger;
}

/**************************************************************************//**
 * @brief  Clears the history of the moving average calculation.
 *
 * @return None.
 *****************************************************************************/
void rangeTestMAClearAll()
{
  rangeTest.maHistory[0u] = rangeTest.maHistory[1u]
                              = rangeTest.maHistory[2u]
                                  = rangeTest.maHistory[3u]
                                      = 0u;
}

/**************************************************************************//**
 * @brief  Returns the moving average of missing pacakges based on the
 *         history data.
 *
 * @return The current moving average .
 *****************************************************************************/
uint8_t rangeTestMAGet()
{
  uint8_t i;
  uint8_t retVal = 0u;

  for (i = 0u; i < (rangeTest.maSize >> 5u); i++) {
    retVal += rangeTestCountBits(rangeTest.maHistory[i]);
  }
  return retVal;
}

/**************************************************************************//**
 * @brief  Resets the internal status of the Range Test.
 *
 * @return None.
 *****************************************************************************/
void rangeTestInit()
{
  rangeTest.pktsCnt = 0u;
  rangeTest.pktsOffset = 0u;
  rangeTest.pktsRcvd = RANGETEST_PACKET_COUNT_INVALID;
  rangeTest.pktsSent = 0u;
  rangeTest.pktsCRC = 0u;
  rangeTest.isRunning = false;
  txReady = true;
  txScheduled = false;

  // Clear RSSI chart
  GRAPHICS_RSSIClear();
}

/*****************************************************************************
* @brief  The function checks how much bytes can be allocated by rail
*         i.e. the return value of RAIL_SetTxFifo() call
*
* @return None.
*****************************************************************************/
static void checkAllocatedFifoSize(uint16_t fifoSize)
{
  if (fifoSize < sizeof(txBuffer)) {
    char uartBuff[64];
    sprintf(uartBuff, "Only %u bytes are allocated by RAIL_SetTxFifo() instead of %u\n", fifoSize, sizeof(txBuffer));
    UARTDRV_Transmit(UARTHandle, (uint8_t *) uartBuff, strlen(uartBuff), NULL);
  }
}

/**************************************************************************//**
 * @brief  Main function of Range Test.
 *****************************************************************************/
int main(void)
{
  uint32_t lastTick = 0u;
  UARTDRV_Init_t uartInit =
  {
    .port = RETARGET_UART,                  /* USART port */
    .baudRate = 115200,                     /* Baud rate */
#if defined(_USART_ROUTELOC0_MASK)
    .portLocationTx = RETARGET_TX_LOCATION, /* USART Tx pin location number */
    -portLocationRx = RETARGET_RX_LOCATION, /* USART Rx pin location number */
#elif defined(_USART_ROUTE_MASK)
    .portLocation = RETARGET_LOCATION,
#endif
#if defined(_GPIO_USART_ROUTEEN_MASK)
    .txPort = RETARGET_TXPORT,
    .txPin = RETARGET_TXPIN,
    .rxPort = RETARGET_RXPORT,
    .rxPin = RETARGET_RXPIN,
    .uartNum = RETARGET_UART_INDEX,
#endif
    .stopBits = (USART_Stopbits_TypeDef)USART_FRAME_STOPBITS_ONE, /* Stop bits */
    .parity = (USART_Parity_TypeDef)USART_FRAME_PARITY_NONE,    /* Parity */
    .oversampling = (USART_OVS_TypeDef)USART_CTRL_OVS_X16,      /* Oversampling mode*/
#if defined(USART_CTRL_MVDIS)
    .mvdis = false,                                             /* Majority vote disable */
#endif
    .fcType = uartdrvFlowControlNone,                           /* Flow control */
#if defined(RETARGET_CTSPORT)
    .ctsPort = RETARGET_CTSPORT,                                /* CTS port number */
#endif
#if defined(RETARGET_CTSPIN)
    .ctsPin = RETARGET_CTSPIN,                                  /* CTS pin number */
#endif
#if defined(RETARGET_RTSPORT)
    .rtsPort = RETARGET_RTSPORT,                                /* RTS port number */
#endif
#if defined(RETARGET_RTSPIN)
    .rtsPin = RETARGET_RTSPIN,                                  /* RTS pin number */
#endif
    .rxQueue = (UARTDRV_Buffer_FifoQueue_t *)NULL,              /* RX operation queue */
    .txQueue = (UARTDRV_Buffer_FifoQueue_t *)&UartTxQueue       /* TX operation queue */
  };

  halInit();

  // Initialize the BSP
  BSP_Init(BSP_INIT_BCC);

  /* Setup GPIO for pushbuttons. */
  GpioSetup();

  railHandle = RAIL_Init(&railCfg, NULL);
  if (railHandle == NULL) {
    while (1) ;
  }

  uint16_t fifoSize = RAIL_SetTxFifo(railHandle, railTxFifo, 0, sizeof(railTxFifo));

  RAIL_ConfigCal(railHandle, RAIL_CAL_ALL);

  // Configure the radio for 2.4 GHz IEEE 802.15.4 (default).
  if (initIEEE802154() != RAIL_STATUS_NO_ERROR) {
    // Error: RAIL IEEE 802.15.4 initialization was not successful
    // Please update your IEEE 802.15.4 configuration
    while (1) ;
  }

  // Configure RAIL callbacks with appended info
  RAIL_ConfigEvents(railHandle, RAIL_EVENTS_ALL, (RAIL_EVENTS_TX_COMPLETION | RAIL_EVENTS_RX_COMPLETION));

  // Initialize the PA now that the HFXO is up and the timing is correct
  RAIL_TxPowerConfig_t txPowerConfig = {
#if HAL_PA_2P4_LOWPOWER
    .mode = RAIL_TX_POWER_MODE_2P4_LP,
#else
    .mode = RAIL_TX_POWER_MODE_2P4_HP,
#endif
    .voltage = BSP_PA_VOLTAGE,
    .rampTime = HAL_PA_RAMP,
  };

  if (RAIL_ConfigTxPower(railHandle, &txPowerConfig) != RAIL_STATUS_NO_ERROR) {
    // Error: The PA could not be initialized due to an improper configuration.
    // Please ensure your configuration is valid for the selected part.
    while (1) ;
  }
  RAIL_SetTxPower(railHandle, HAL_PA_POWER);

  GRAPHICS_Init();
  pbInit();
  menuInit();
  seqInit();

  halInternalInitVCPPins();
  // RTC Init -- for system timekeeping and other useful things
  // this has to be called after GRAPHICS_Init()
  sl_sleeptimer_init();
  UARTDRV_Init(UARTHandle, &uartInit);
  GPIOINT_Init();

  rangeTestInit();

  UARTDRV_Transmit(UARTHandle, (uint8_t *) "\nRange Test EFR32\n\n", 19u, NULL);

  checkAllocatedFifoSize(fifoSize);

  while (1u) {
    if ((sl_sleeptimer_tick_to_ms(sl_sleeptimer_get_tick_count64()) - lastTick) >= 5u) {
      // 5ms loop

      pbPoll();
      seqRun();

      // Callback to replace RTCDRV stuff
      if (repeatCallbackFn != NULL) {
        static uint32_t cnt;

        if (cnt++ >= repeatCallbackTimeout) {
          repeatCallbackFn();
          cnt = 0u;
        }
      }

      lastTick = sl_sleeptimer_tick_to_ms(sl_sleeptimer_get_tick_count64());
    }

    if (pktLog) {
      // Print log info
      char buff[115u];

      rangeTest_t rangeTestBuffered;
      memcpy((void*)&rangeTestBuffered, (void*)&rangeTest, sizeof(rangeTest));
      if (RADIO_MODE_RX == rangeTest.radioMode) {
        sprintf(buff, "Rcvd, "          //6
                      "OK:%u, "         //10
                      "CRC:%u, "        //11
                      "Sent:%u, "       //12
                      "Payld:%u, "      //10
                      "MASize:%u, "     //12
                      "PER:%3.1f, "     //11
                      "MA:%3.1f, "      //10
                      "RSSI:% 3d, "    //12
                      "IdS:%u, "        //8
                      "IdR:%u"          //8
                      "\n",             //1+1
                rangeTestBuffered.pktsRcvd,
                rangeTestBuffered.pktsCRC,
                rangeTestBuffered.pktsCnt,
                rangeTestBuffered.payloadLength,
                rangeTestBuffered.maSize,
                rangeTestBuffered.PER,
                rangeTestBuffered.MA,
                (int8_t)rangeTestBuffered.rssiLatch,
                rangeTestBuffered.srcID,
                rangeTestBuffered.destID);
      }

      if (RADIO_MODE_TX == rangeTest.radioMode) {
        sprintf(buff,
                "Sent, Actual:%u, Max:%u, IdS:%u, IdR:%u\n",
                rangeTestBuffered.pktsSent,
                rangeTestBuffered.pktsReq,
                rangeTestBuffered.srcID,
                rangeTestBuffered.destID);
      }
      UARTDRV_Transmit(UARTHandle, (uint8_t *) buff, strlen(buff), NULL);

      pktLog = false;
    }
  }
}

/*****************************************************************************
* @brief  The function initializes IEEE80215.4, 2.4GHz (250kbps)
*
* @return RAIL_Status_t, the result of the RAIL API calls
*****************************************************************************/
static RAIL_Status_t initIEEE802154()
{
  RAIL_Status_t rs = RAIL_STATUS_NO_ERROR;
  physicalCh = IEEE802154_CHANNEL;

  // Configure the radio and channels for 2.4 GHz IEEE 802.15.4.
  rs = RAIL_IEEE802154_Config2p4GHzRadio(railHandle);
  if (rs == RAIL_STATUS_NO_ERROR) {
    // Initialize the IEEE 802.15.4 configuration using the static configuration above.
    rs = RAIL_IEEE802154_Init(railHandle, &rail154Config);
    if (rs == RAIL_STATUS_NO_ERROR) {
      rangeTest.lastPHY = IEEE802154_250KBPS;
    }
  }

  return rs;
}

/*****************************************************************************
* @brief  The function initializes 4 predefined BLE PHYs:
*         - BLE, 125kbps
*         - BLE, 500kbps
*         - BLE, 1Mbps
*         - BLE, 2Mbps
*
* @return RAIL_Status_t, the result of the RAIL API calls
*****************************************************************************/
static RAIL_Status_t initBLEPHY()
{
  RAIL_Status_t rs = RAIL_STATUS_NO_ERROR;
  physicalCh = BLE_PHYSICAL_CH;
  logicalCh = BLE_LOGICAL_CH;

  // Configure BLE PHY
  RAIL_BLE_Init(railHandle);
  if (rs == RAIL_STATUS_NO_ERROR) {
    switch (rangeTest.currentPHY) {
      case BLE_125KBPS:
        rs = RAIL_BLE_ConfigPhyCoded(railHandle, RAIL_BLE_Coding_125kbps);
        break;
      case BLE_500KBPS:
        rs = RAIL_BLE_ConfigPhyCoded(railHandle, RAIL_BLE_Coding_500kbps);
        break;
      case BLE_1MBPS:
        rs = RAIL_BLE_ConfigPhy1MbpsViterbi(railHandle);
        break;
      case BLE_2MBPS:
        rs = RAIL_BLE_ConfigPhy2MbpsViterbi(railHandle);
        break;
    }
    if (rs == RAIL_STATUS_NO_ERROR) {
      rs = RAIL_BLE_ConfigChannelRadioParams(railHandle, BLE_CRC_INIT, BLE_ACCESS_ADDRESS, logicalCh, DISABLE_WHITENING);
      if (rs == RAIL_STATUS_NO_ERROR) {
        rangeTest.lastPHY = rangeTest.currentPHY;
      }
    }
  }

  return rs;
}

/*****************************************************************************
* @brief  The function initializes 5 predefined PHYs:
*         - BLE, 125kbps
*         - BLE, 500kbps
*         - BLE, 1Mbps
*         - BLE, 2Mbps
*         - IEEE80215.4, 2.4GHz (250kbps)
*
* @return RAIL_Status_t, the result of the RAIL API calls
*****************************************************************************/
static RAIL_Status_t initCurrentPHY()
{
  RAIL_Status_t rs = RAIL_STATUS_NO_ERROR;

  RAIL_RadioState_t radioState = RAIL_GetRadioState(railHandle);

  if (radioState == RAIL_RF_STATE_IDLE) {
    if (rangeTest.currentPHY == IEEE802154_250KBPS) {
      initIEEE802154();
    } else {
      initBLEPHY();
    }
  } else {
    // Initialization shall be applied in Idle
    rs = RAIL_STATUS_INVALID_STATE;
  }

  return rs;
}

/*****************************************************************************
* @brief  The function prepares the following BLE PDU:
*         - ADV_NONCONN_IND
*         - LL advertiser address
*         - AD Structure: Flags
*         - AD Structure: Manufacturer specific
*           - Company ID
*           - Structure type; used for backward compatibility
*           - rangeTestPacket_t
*           - 0x55, 0xAA, 0x55, 0xAA... (only if payload length is more than sizeof(rangeTestPacket_t))
*
* @return None
*****************************************************************************/
static void prepareBLEAdvertisingChannelPDU(void)
{
  AdvNonconnInd_t* pBleTxPdu = (AdvNonconnInd_t*)txBuffer;

  // BLE advertisement header
  pBleTxPdu->header.type = BLE_HEADER_LSB;   //ADV_NONCONN_IND
  //LL header, LL advertiser's address, LL advertisement data
  pBleTxPdu->header.length = sizeof(pBleTxPdu->header.type)
                             + sizeof(pBleTxPdu->advAddr)
                             + sizeof(pBleTxPdu->flags)
                             + sizeof(pBleTxPdu->manufactSpec.length)
                             + sizeof(pBleTxPdu->manufactSpec.adType)
                             + sizeof(pBleTxPdu->manufactSpec.companyID)
                             + sizeof(pBleTxPdu->manufactSpec.version)
                             + rangeTest.payloadLength;
  // LL advertiser's address
  pBleTxPdu->advAddr[0] = 0xC1;
  pBleTxPdu->advAddr[1] = 0x29;
  pBleTxPdu->advAddr[2] = 0xD8;
  pBleTxPdu->advAddr[3] = 0x57;
  pBleTxPdu->advAddr[4] = 0x0B;
  pBleTxPdu->advAddr[5] = 0x00;
  // AD Structure: Flags
  pBleTxPdu->flags.length = sizeof(pBleTxPdu->flags.adType) + sizeof(pBleTxPdu->flags.flags);   // Length of field: Type + Flags
  pBleTxPdu->flags.adType = ADSTRUCT_TYPE_FLAG;   // AD type: Flags
  pBleTxPdu->flags.flags = DISABLE_BR_EDR | LE_GENERAL_DISCOVERABLE_MODE;   // Flags: BR/EDR is disabled, LE General Discoverable Mode
  // AD Structure: Manufacturer specific
  pBleTxPdu->manufactSpec.length = sizeof(pBleTxPdu->manufactSpec.adType)
                                   + sizeof(pBleTxPdu->manufactSpec.companyID)
                                   + sizeof(pBleTxPdu->manufactSpec.version)
                                   + rangeTest.payloadLength;
  pBleTxPdu->manufactSpec.adType = ADSTRUCT_TYPE_MANUFACTURER_SPECIFIC;   // AD type: Manufacturer Specific Data
  pBleTxPdu->manufactSpec.companyID = COMPANY_ID;
  pBleTxPdu->manufactSpec.version = 0x01;
  // RangeTest payload
  pBleTxPdu->manufactSpec.payload.pktCounter = rangeTest.pktsSent;
  pBleTxPdu->manufactSpec.payload.destID = rangeTest.destID;
  pBleTxPdu->manufactSpec.payload.srcID = rangeTest.srcID;
  pBleTxPdu->manufactSpec.payload.repeat = 0xFF;
  if (rangeTest.payloadLength > PAYLOAD_LEN_MIN) {
    rangeTestGenerateRemainder(pBleTxPdu->manufactSpec.remainder);
  }
}

/*****************************************************************************
* @brief  The function prepares a std. IEEE 802.15.4 Data frame format
* Data frame format
* - MHR:
*   - Frame Control (2 bytes)
*   - Sequence number (0/1)
*   - Addressing fields (variable)
*   - Auxiliary Security Header (variable)
*   - Header IEs (variable)
* - MAC Payload:
*   - Payload IEs (variable)
*   - Data Payload (variable)
* - MFR (2/4)
* @return None
*****************************************************************************/
static void prepareIEEE802154DataFrame()
{
  uint8_t i = 0;
  dataFrameFormat_t* pDataFrame;

  // PHR - Length field (byte0) is not included but the CRC (2 bytes)
  txBuffer[i++] = sizeof(mhr_t) + rangeTest.payloadLength + IEEE802154_CRC_LENGTH;
  // Payload length is counted from txBuffer[1] (byte0: Length field)
  pDataFrame = (dataFrameFormat_t*) &txBuffer[i];
  pDataFrame->mhr.frameControl = FRAME_CONTROL;
  pDataFrame->mhr.sequenceNum = rangeTest.pktsSent;
  pDataFrame->mhr.destPANID = 0xFFFF;
  pDataFrame->mhr.srcAddr = 0x0000;
  pDataFrame->mhr.destAddr = 0xFFFF;
  pDataFrame->payload.srcID = rangeTest.srcID;
  pDataFrame->payload.destID = rangeTest.destID;
  pDataFrame->payload.pktCounter = rangeTest.pktsSent;
  pDataFrame->payload.repeat = 0x00;
  if (rangeTest.payloadLength > PAYLOAD_LEN_MIN) {
    rangeTestGenerateRemainder(&pDataFrame->remainder[0]);
  }
}

/*****************************************************************************
* @brief  The function prepares payload depending on the currently set PHY
*         and starts transmission
*
* @return None
*****************************************************************************/
static void preparePayloadStartTx()
{
  char uartBuff[32u];

  if (rangeTest.currentPHY == IEEE802154_250KBPS) {
    prepareIEEE802154DataFrame();

    // Including length field (1 byte)
    uint8_t bufferDataLength = sizeof(mhr_t) + rangeTest.payloadLength + IEEE802154_PHR_LENGTH;
    if (RAIL_WriteTxFifo(railHandle, txBuffer, bufferDataLength, true) != bufferDataLength) {
      sprintf(uartBuff, "RAIL_WriteTxFifo() error\n");
      UARTDRV_Transmit(UARTHandle, (uint8_t *) uartBuff, strlen(uartBuff), NULL);
    } else {
      RAIL_Status_t txState = RAIL_StartTx(railHandle, physicalCh, RAIL_TX_OPTIONS_DEFAULT, NULL);

      if (txState != RAIL_STATUS_NO_ERROR) {
        sprintf(uartBuff, "RAIL_StartTx() error \n");
        UARTDRV_Transmit(UARTHandle, (uint8_t *) uartBuff, strlen(uartBuff), NULL);
      }
    }
  } else {
    prepareBLEAdvertisingChannelPDU();

    if (RAIL_WriteTxFifo(railHandle, txBuffer, txBuffer[1] + 2, true) != txBuffer[1] + 2) {
      sprintf(uartBuff, "RAIL_WriteTxFifo() error\n");
      UARTDRV_Transmit(UARTHandle, (uint8_t *) uartBuff, strlen(uartBuff), NULL);
    } else {
      RAIL_Status_t txState = RAIL_StartTx(railHandle, physicalCh, RAIL_TX_OPTIONS_DEFAULT, NULL);
      if (txState != RAIL_STATUS_NO_ERROR) {
        sprintf(uartBuff, "RAIL_StartTx() error \n");
        UARTDRV_Transmit(UARTHandle, (uint8_t *) uartBuff, strlen(uartBuff), NULL);
      }
    }
  }
}

static rangeTestPacket_t* getStartOfPayload(uint8_t* buf)
{
  char uartBuff[14u];
  rangeTestPacket_t* payload = NULL;
  if (rangeTest.currentPHY == IEEE802154_250KBPS) {
    // 1st byte is the length field (PHR)
    dataFrameFormat_t* pDataFrame = (dataFrameFormat_t *) &buf[1];
    payload = &pDataFrame->payload;
  } else if (rangeTest.currentPHY == BLE_125KBPS
             || rangeTest.currentPHY == BLE_500KBPS
             || rangeTest.currentPHY == BLE_1MBPS
             || rangeTest.currentPHY == BLE_2MBPS) {
    payload = (rangeTestPacket_t*)&buf[16];
  } else {
    // Can not happen
    sprintf(uartBuff, "Unknown PHY \n");
    UARTDRV_Transmit(UARTHandle, (uint8_t *) uartBuff, strlen(uartBuff), NULL);
  }
  return payload;
}

static void registerEvents(railEvents_t* railEvents, RAIL_Events_t events)
{
  if (events & RAIL_EVENT_RSSI_AVERAGE_DONE) {
    railEvents->rssiAverageDone++;
  }
  if (events & RAIL_EVENT_RX_ACK_TIMEOUT) {
    railEvents->rxAckTimeout++;
  }
  if (events & RAIL_EVENT_RX_FIFO_ALMOST_FULL) {
    railEvents->rxFifoAlmostFull++;
  }
  if (events & RAIL_EVENT_RX_PACKET_RECEIVED) {
    railEvents->rxPacketReceived++;
  }
  if (events & RAIL_EVENT_RX_PREAMBLE_LOST) {
    railEvents->rxPreambleLost++;
  }
  if (events & RAIL_EVENT_RX_PREAMBLE_DETECT) {
    railEvents->rxPreambleDetect++;
  }
  if (events & RAIL_EVENT_RX_SYNC1_DETECT) {
    railEvents->rxSync1Detect++;
  }
  if (events & RAIL_EVENT_RX_SYNC2_DETECT) {
    railEvents->rxSync2Detect++;
  }
  if (events & RAIL_EVENT_RX_FRAME_ERROR) {
    railEvents->rxFrameError++;
  }
  if (events & RAIL_EVENT_RX_FIFO_OVERFLOW) {
    railEvents->rxFifoOverflow++;
  }
  if (events & RAIL_EVENT_RX_ADDRESS_FILTERED) {
    railEvents->rxAddressFiltered++;
  }
  if (events & RAIL_EVENT_RX_TIMEOUT) {
    railEvents->rxTimeout++;
  }
  if (events & RAIL_EVENT_RX_SCHEDULED_RX_END) {
    railEvents->rxScheduledRxEnd++;
  }
  if (events & RAIL_EVENT_RX_PACKET_ABORTED) {
    railEvents->rxPacketAborted++;
  }
  if (events & RAIL_EVENT_RX_FILTER_PASSED) {
    railEvents->rxFilterPassed++;
  }
  if (events & RAIL_EVENT_RX_TIMING_LOST) {
    railEvents->rxTimingLost++;
  }
  if (events & RAIL_EVENT_RX_TIMING_DETECT) {
    railEvents->rxTimingDetect++;
  }
  if (events & RAIL_EVENT_RX_CHANNEL_HOPPING_COMPLETE) {
    railEvents->rxChannelHoppingComplete++;
  }
  if (events & RAIL_EVENT_IEEE802154_DATA_REQUEST_COMMAND) {
    railEvents->ieee802154DataRequestCommand++;
  }
  if (events & RAIL_EVENT_ZWAVE_BEAM) {
    railEvents->zwaveBeam++;
  }
  if (events & RAIL_EVENT_TX_FIFO_ALMOST_EMPTY) {
    railEvents->txFifoAlmostEmpty++;
  }
  if (events & RAIL_EVENT_TX_PACKET_SENT) {
    railEvents->txPacketSent++;
  }
  if (events & RAIL_EVENT_TXACK_PACKET_SENT) {
    railEvents->txackPacketSent++;
  }
  if (events & RAIL_EVENT_TX_ABORTED) {
    railEvents->txAborted++;
  }
  if (events & RAIL_EVENT_TXACK_ABORTED) {
    railEvents->txackAborted++;
  }
  if (events & RAIL_EVENT_TX_BLOCKED) {
    railEvents->txBlocked++;
  }
  if (events & RAIL_EVENT_TXACK_BLOCKED) {
    railEvents->txackBlocked++;
  }
  if (events & RAIL_EVENT_TX_UNDERFLOW) {
    railEvents->txUnderflow++;
  }
  if (events & RAIL_EVENT_TXACK_UNDERFLOW) {
    railEvents->txackUnderflow++;
  }
  if (events & RAIL_EVENT_TX_CHANNEL_CLEAR) {
    railEvents->txChannelClear++;
  }
  if (events & RAIL_EVENT_TX_CHANNEL_BUSY) {
    railEvents->txChannelBusy++;
  }
  if (events & RAIL_EVENT_TX_CCA_RETRY) {
    railEvents->txCcaRetry++;
  }
  if (events & RAIL_EVENT_TX_START_CCA) {
    railEvents->txStartCca++;
  }
  if (events & RAIL_EVENT_CONFIG_UNSCHEDULED) {
    railEvents->configUnscheduled++;
  }
  if (events & RAIL_EVENT_CONFIG_SCHEDULED) {
    railEvents->configScheduled++;
  }
  if (events & RAIL_EVENT_SCHEDULER_STATUS) {
    railEvents->schedulerStatus++;
  }
  if (events & RAIL_EVENT_CAL_NEEDED) {
    railEvents->calNeeded++;
  }
}
/**************************************************************************//**
 * @brief  Function to execute Range Test functions, depending on the
 *         selected mode (TX or RX).
 *
 * @return None.
 *****************************************************************************/
bool runDemo()
{
  // Range Test runner
  uint8_t retVal = false;

  if (rangeTest.isRunning) {
    // Started

    switch (rangeTest.radioMode) {
      case RADIO_MODE_RX:
        if (pktRx) {
          // Refresh screen
          pktRx = false;

          retVal = true;
        }
        break;

      case RADIO_MODE_TX:
      {
        // Buffering volatile field
        uint16_t pktsSent = rangeTest.pktsSent;
        uint16_t pktsReq = rangeTest.pktsReq;

        if (pktsSent < pktsReq) {
          // Need to send more packets
          if (txReady & txScheduled) {
            txReady = false;

            rangeTest.pktsSent++;
            if (rangeTest.pktsSent > 50000u) {
              rangeTest.pktsSent = 1u;
            }

            preparePayloadStartTx();

            // Refresh screen
            retVal = true;
          }
        } else {
          // Requested amount of packets has been sent
          rangeTest.isRunning = false;

          // Refresh screen
          retVal = true;
        }
        break;
      }
      case RADIO_MODE_TRX:
        break;

      default:
        //assert!
        break;
    }
  } else {
    // Stopped

    // Initialization is done only if a new kind of PHY is set
    if (rangeTest.lastPHY != rangeTest.currentPHY) {
      RAIL_Status_t rs = initCurrentPHY();
      if (rs != RAIL_STATUS_NO_ERROR) {
        UARTDRV_Transmit(UARTHandle, (uint8_t *) "\nPHY initialization failed\n\n", 28u, NULL);
      }
    } else {
      // Currently set predefined PHY is the same as the lastly initialized
      // hence re-initialization is not needed
    }

    if (RAIL_RF_STATE_IDLE != RAIL_GetRadioState(railHandle)) {
      RAIL_Idle(railHandle, RAIL_IDLE, true);
    }

    pktRx = false;
    txReady = true;
    // Stop transmitting packets.
    txScheduled = false;

    if (RADIO_MODE_RX == rangeTest.radioMode) {
      // Can't stop RX
      rangeTest.isRunning = true;

      // Kick-start RX
      RAIL_StartRx(railHandle, physicalCh, NULL);
    }
  }

  return retVal;
}

void rangeTestStartPeriodicTx(void)
{
  if (sl_sleeptimer_start_periodic_timer(&periodicTimer,
                                         RANGETEST_TX_PERIOD,
                                         periodicTimerCallback,
                                         NULL,
                                         1u,
                                         0u) != SL_STATUS_OK) {
    while (1) ;
  }
}

void rangeTestStopPeriodicTx(void)
{
  if (sl_sleeptimer_stop_timer(&periodicTimer) != SL_STATUS_OK) {
    while (1) ;
  }
}

static void periodicTimerCallback(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void)handle;
  (void)data;
  // Schedule proprietary packet transmission.
  txScheduled = true;
}
