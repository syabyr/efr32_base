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

#ifndef RANGETEST_H_
#define RANGETEST_H_

#include "bsp.h"
#include "retargetserialhalconfig.h"
#include "rail_types.h"
#include "rail_chip_specific.h"
#include "rangetestconfigtypes.h"
#include "pushButton.h"

// ----------------------------------------------------------------------------
// Defines

// Time spacing between two packets in ms
#define RANGETEST_TX_PERIOD               100

// Invalid counter value
#define RANGETEST_PACKET_COUNT_INVALID    0xFFFF

// Repeate TX mode
#define RANGETEST_TX_REPEAT               0xFFFF

// RAIL buffer sizes
#define RAIL_TX_BUFFER_SIZE         (128u)
#define RAIL_RX_BUFFER_SIZE         (256u)

// Tx output power min/max values in the LCD
#define TX_POWER_MAX           (200)     // 0.1 dBm / LSB
#define TX_POWER_MIN           (-150)    // 0.1 dBm / LSB
// Output power increment when the button is pressed
#define TX_POWER_INC           (5)       // 0.1 dBm / LSB

// BLE macros
#define BLE_PHYSICAL_CH    (0u)
#define BLE_LOGICAL_CH     (37u)
#define BLE_CRC_INIT       (0x555555)
#define BLE_ACCESS_ADDRESS (0x12345678) //0x8E89BED6
#define ADV_NONCONN_IND    (0x02)
#define BLE_PDU_TYPE       (ADV_NONCONN_IND)
#define BLE_HEADER_LSB     (BLE_PDU_TYPE)      // RFU(0)|ChSel(0)|TxAdd(0)|RxAdd(0)
#define COMPANY_ID         ((uint16_t) 0x02FF) // Company Identifier of Silicon Labs
#define DISABLE_WHITENING  (false)
// ADV_NONVONN_IND::Adv Data (32 Bytes)
// - AD struct: Flags (3 Bytes)
// - AD struct: Manufacturer specific static part (10 Bytes):
// Length (1 Byte)
// Company ID (2 Bytes)
// Struct version (1 Byte)
// pktSent + destId + srcId + repeat (5 Bytes)
// Max remaining (0x55, 0xAA, 0x55, etc.) (19 Bytes)
#define BLE_PAYLOAD_LEN_MAX          (24u)
// AD Structure: Flags
#define ADSTRUCT_TYPE_FLAG           (0x01)
#define DISABLE_BR_EDR               (0x04)
#define LE_GENERAL_DISCOVERABLE_MODE (0x02)
// AD Structure: Manufacturer Specific
#define ADSTRUCT_TYPE_MANUFACTURER_SPECIFIC (0xFF)

// IEEE 802.15.4 macros
#define IEEE802154_CHANNEL           (11u)    // 11 is the first 1st 2.4GHz channel
// Indicates the effective payload length excluding CRC (2 Bytes)
// TODO BTAPP-578, BG-6270, FLEX-1127
// #define IEEE802154_PAYLOAD_LEN_MAX   (116u)   // 127bytes - 2bytes (CRC) - 9bytes (Data Frame::MHR)
#define IEEE802154_PAYLOAD_LEN_MAX   (24u)   // 127bytes - 2bytes (CRC) - 9bytes (Data Frame::MHR)
#define IEEE802154_CRC_LENGTH        (2u)     // IEEE 802.15.4 CRC length is 2 bytes
#define IEEE802154_PHR_LENGTH        (1u)     // IEEE 802.15.4 PHR length is 1 byte
// Data Frame Format::Frame Control (2 bytes)
#define FRAME_TYPE                   (0x0001) // Data Frame
#define SECURITY_ENABLED             (0x0000) // Not enabled
#define FRAME_PENDING                (0x0000) // Not enabled
#define AR                           (0x0000) // Ack not required
#define PAN_ID_COMPRESSION           (0x0040) // Enabled
#define SEQUENCE_NUMBER_SUPPRESSION  (0x0000) // Sequence number is not suppressed
#define IE_PRESENT                   (0x0000) // IEs is not contained
#define DESTINATION_ADDRESSING_MODE  (0x0800) // Address field contains a short address (16 bit)
#define FRAME_VERSION                (0x2000) // IEEE Std 802.15.4
#define SOURCE_ADDRESSING_MODE       (0x8000) // Address field contains a short address (16 bit)
#define FRAME_CONTROL                (FRAME_TYPE | SECURITY_ENABLED | FRAME_PENDING           \
                                      | AR | PAN_ID_COMPRESSION | SEQUENCE_NUMBER_SUPPRESSION \
                                      | IE_PRESENT | DESTINATION_ADDRESSING_MODE | FRAME_VERSION | SOURCE_ADDRESSING_MODE)

// BLE and IEEE 802.15.4 common macros
// Default payload length value
#define PAYLOAD_LEN_MIN   (sizeof(rangeTestPacket_t))

// RSSI chart Y scale values
#define RADIO_CONFIG_RSSI_MIN_VALUE "-115"
#define RADIO_CONFIG_RSSI_MID_VALUE "-42"
#define RADIO_CONFIG_RSSI_MAX_VALUE "10"

// In case of Series 2 (RD4171A,BRD4180A and BRD4181A) channelSpacing can not be set
#define NO_CHANNEL_SPACING  (0u)
#define _2450_MHZ           ((uint16_t) 2450u)

// ----------------------------------------------------------------------------
// Types

/// Enumeration of the Predefined PHYs. Newly added predefined PHY shall be placed before _NUM_OF_PREDEFINED_PHYS
typedef enum {
  IEEE802154_250KBPS,    ///> IEEE80215.4, 250kbps
  BLE_125KBPS,           ///> BLE 125kbps
  BLE_500KBPS,           ///> BLE 500kbps
  BLE_1MBPS,             ///> BLE 1Mbps
  BLE_2MBPS,             ///> BLE 2Mbps
  NUM_OF_PREDEFINED_PHYS
} predefinedPHY_e;

/// This enumeration contains the supported modes of the Range Test.
typedef enum {
  RADIO_MODE_TRX,   ///> Transceiver mode, unused.
  RADIO_MODE_RX,    ///> Receiver Mode. Device listens for packet.
  RADIO_MODE_TX,    ///> Transmitter Mode. Device sends packets.
  NUMOF_RADIO_MODES ///> Number of enum states
} sRadioModes_e;

/// Range Test status variables that contain settings and data that is
/// used during the Range Test execution.
typedef struct rangeTest_t{
  float    PER;           ///> Current Packet Error Rate.
  float    MA;            ///> Current Moving Average.
  uint32_t maHistory[4u]; ///> Array that stores history to calculate MA.
  uint16_t pktsSent;      ///> Number of sent packets.
  uint16_t pktsCnt;       ///> Counter in received packet.
  uint16_t pktsRcvd;      ///> Number of CRC OK packets.
  uint16_t pktsOffset;    ///> Counter offset in first received packet.
  uint16_t pktsReq;       ///> Number of requested packets to send.
  uint16_t pktsCRC;       ///> Number of CRC error packets.
  uint16_t channel;       ///> Selected channel for TX and RX.
  uint8_t  radioMode;     ///> TX or RX operation.
  uint8_t  currentPHY;    ///> Currently set, still not initialized predefined PHY (e.g. BLE GFSK: 125kbps)
  uint8_t  lastPHY;       ///> Lastly initialized predefined PHY (e.g. BLE GFSK: 125kbps)
  int16_t  txPower;       ///> Radio transmit power in 0.1 dBm steps
  uint8_t  destID;        ///> ID of the other device to send or receive.
  uint8_t  srcID;         ///> ID of this device.
  uint8_t  payloadLength; ///> Payload length of the packets.
  uint8_t  retransmitCnt;
  uint8_t  rssiMode;
  uint8_t  rssiLatch;     ///> RSSI value logged for the last RX packet.
  uint8_t  maSize;        ///> Moving Average window size (no. of values).
  uint8_t  maFinger;      ///> Points to current value in the MA window.
  bool     log;           ///> True if UART logging is enabled in menu.
  bool     isRunning;     ///> True if the Range Test is running.
} rangeTest_t;

/// The struct is used for registering the rail events. Can be used during development and debugging
typedef struct railEvents_t {
  // Rx Event Bits
  uint8_t rssiAverageDone;
  uint8_t rxAckTimeout;
  uint8_t rxFifoAlmostFull;
  uint8_t rxPacketReceived;
  uint8_t rxPreambleLost;
  uint8_t rxPreambleDetect;
  uint8_t rxSync1Detect;
  uint8_t rxSync2Detect;
  uint8_t rxFrameError;
  uint8_t rxFifoOverflow;
  uint8_t rxAddressFiltered;
  uint8_t rxTimeout;
  uint8_t rxScheduledRxEnd;
  uint8_t rxPacketAborted;
  uint8_t rxFilterPassed;
  uint8_t rxTimingLost;
  uint8_t rxTimingDetect;
  uint8_t rxChannelHoppingComplete;
  uint8_t ieee802154DataRequestCommand;
  uint8_t zwaveBeam;

  // Tx Event Bits
  uint8_t txFifoAlmostEmpty;
  uint8_t txPacketSent;
  uint8_t txackPacketSent;
  uint8_t txAborted;
  uint8_t txackAborted;
  uint8_t txBlocked;
  uint8_t txackBlocked;
  uint8_t txUnderflow;
  uint8_t txackUnderflow;
  uint8_t txChannelClear;
  uint8_t txChannelBusy;
  uint8_t txCcaRetry;
  uint8_t txStartCca;

  // Scheduler Event Bits
  uint8_t configUnscheduled;
  uint8_t configScheduled;
  uint8_t schedulerStatus;

  // Other Event Bits
  uint8_t calNeeded;
} railEvents_t;

#pragma pack(1)
/// Packet structure.
typedef struct rangeTestPacket_t{
  uint16_t pktCounter;    ///> Value showing the number of this packet.
  uint8_t  destID;        ///> Destination device ID this packet was sent to.
  uint8_t  srcID;         ///> Device ID which shows which device sent this packet.
  uint8_t  repeat;        ///> Unused.
} rangeTestPacket_t;

// IEEE 802.15.4 Data Frame:: MHR
typedef struct mhr_t{
  uint16_t frameControl; ///> Frame Control
  uint8_t  sequenceNum;  ///> Sequence Number
  uint16_t destPANID;    ///> Destination PAN ID
  uint16_t destAddr;     ///> Destination Address
  uint16_t srcAddr;      ///> Source Address
}mhr_t;

// IEEE 802.15.4 Data Frame
typedef struct dataFrameFormat_t{
  mhr_t mhr; ///> Data Frame:: MHR
  rangeTestPacket_t payload; ///> Payload of the Data Frame
  uint8_t remainder[IEEE802154_PAYLOAD_LEN_MAX - PAYLOAD_LEN_MIN]; ///> Payload, filled with 0x55, 0xAA
} dataFrameFormat_t;

typedef struct {
  uint8_t type;
  uint8_t length;
} AdvChannelPDUHeader_t;

typedef struct {
  uint8_t length;
  uint8_t adType;
  uint8_t flags;
} ADStructFlags_t;

typedef struct {
  uint8_t length;
  uint8_t adType;
  uint16_t companyID;
  uint8_t version;
  rangeTestPacket_t payload; ///> Payload of the Data Frame
  uint8_t  remainder[BLE_PAYLOAD_LEN_MAX - PAYLOAD_LEN_MIN]; ///> Remainder available in BLE advertisement PDU.
} ADStructManufactSpec_t;

typedef struct {
  AdvChannelPDUHeader_t header;
  uint8_t advAddr[6];
  ADStructFlags_t flags;
  ADStructManufactSpec_t manufactSpec;
} AdvNonconnInd_t;
#pragma pack()

// External RAIL handle, used to call RAIL functions
extern RAIL_Handle_t railHandle;

// External static Radio configuration
extern const uint8_t radio_config[];

//extern RAIL_ChannelConfig_t channelConfig;
extern volatile rangeTest_t rangeTest;

// ----------------------------------------------------------------------------
// Function declarations

void    rangeTestInit();
uint8_t rangeTestMAGet();
bool    runDemo();
void    rangeTestStartPeriodicTx(void);
void    rangeTestStopPeriodicTx(void);

#endif /* RANGETEST_H_ */
