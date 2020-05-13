/***************************************************************************//**
 * @brief Dynamic multiprotocol connect switch sample application.
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

#include "stack/include/ember.h"
#ifndef UNIX_HOST
#include "heartbeat/heartbeat.h"
#endif
#include "hal/hal.h"
#include "poll/poll.h"
#include "command-interpreter/command-interpreter2.h"
#include "debug-print/debug-print.h"
#include "plugin-common/dmp-demo-ui/dmp-ui.h"
#include "dmp-demo-utils/dmp-demo-utils.h"
#include <kernel/include/os.h>

#include <common/include/rtos_utils.h>
#include <kernel/include/os.h>
#include "rtos_bluetooth.h"
#include "rtos_gecko.h"
#include "rtos_utils.h"
#include "gatt_db.h"
#include "rail_config.h"

#include EMBER_AF_API_NVM3
#include "hal/hal.h"
#include "serial/serial.h"

#include "application/app-join.h"

#define TICK_INTERVAL_MS 1000
#define DMP_CONNECT_SWITCH_TX_POWER         0
#define DMP_CONNECT_SWITCH_PAN_ID           0xDEED
#define DMP_CONNECT_SWITCH_PROTOCOL_ID      0xC00E
#define DMP_CONNECT_SWITCH_MASTER_ID        EMBER_COORDINATOR_ADDRESS
#define DMP_CONNECT_SWITCH_SLAVE_ID         (EMBER_COORDINATOR_ADDRESS + 1)
#define SCAN_RETRY_ATTEMPT                3
#define DMP_CONNECT_SWITCH_PROTOCOL_ID_OFFSET  0
#define DMP_CONNECT_SWITCH_COMMAND_ID_OFFSET   2
#define DMP_CONNECT_SWITCH_DATA_OFFSET         3
#define DMP_CONNECT_SWITCH_MINIMUM_LENGTH      DMP_CONNECT_SWITCH_DATA_OFFSET
#define DMP_CONNECT_SWITCH_HEADER_LENGTH       DMP_CONNECT_SWITCH_DATA_OFFSET

#define NETWORK_UP_LED                       BOARDLED0
#define LED_0                                BOARDLED0
#define LED_1                                BOARDLED1
#define DMP_CONNECT_SWITCH_SECURITY_KEY        { 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, \
                                                 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, \
                                                 0xAA, 0xAA, 0xAA, 0xAA }

#define CHANNEL_NUMBER_START                 (channelConfigs[0]->configs->channelNumberStart)
#define CHANNEL_NUMBER_END                   (channelConfigs[0]->configs->channelNumberEnd)
/* Write response codes*/
#define ES_WRITE_OK                         0
#define ES_ERR_CCCD_CONF                    0x81
#define ES_ERR_PROC_IN_PROGRESS             0x80
#define ES_NO_CONNECTION                    0xFF
// Advertisement data
#define UINT16_TO_BYTES(n)        ((uint8_t) (n)), ((uint8_t)((n) >> 8))
#define UINT16_TO_BYTE0(n)        ((uint8_t) (n))
#define UINT16_TO_BYTE1(n)        ((uint8_t) ((n) >> 8))
// Ble TX test macros and functions
#define BLE_TX_TEST_DATA_SIZE   2

#define UUID_LEN 16 // 128-bit UUID

#define IBEACON_MAJOR_NUM 0x0400 // 16-bit major number

#define LE_GAP_MAX_DISCOVERABLE_MODE   0x04
#define LE_GAP_MAX_CONNECTABLE_MODE    0x03
#define LE_GAP_MAX_DISCOVERY_MODE      0x02
#define BLE_INDICATION_TIMEOUT         30000
#define BUTTON_LONG_PRESS_TIME_MSEC    3000

#define LIGHT_STATE_GATTDB     gattdb_light_state
#define TRIGGER_SOURCE_GATTDB  gattdb_trigger_source
#define SOURCE_ADDRESS_GATTDB  gattdb_source_address
#define OTA_CONTROL_GATTDB     gattdb_ota_control

#define DEMO_CONTROL_PAYLOAD_CMD_DATA                  (0x0F)
#define DEMO_CONTROL_PAYLOAD_SRC_ROLE_BIT              (0x80)
#define DEMO_CONTROL_PAYLOAD_SRC_ROLE_BIT_SHIFT        (7)
#define DEMO_CONTROL_PAYLOAD_CMD_MASK                  (0x70)
#define DEMO_CONTROL_PAYLOAD_CMD_MASK_SHIFT            (4)
// Timer Frequency used.
#define TIMER_CLK_FREQ ((uint32)32768)
#define TIMER_S_2_TIMERTICK(s) (TIMER_CLK_FREQ * s)

// We need to put the device name into a scan response packet,
// since it isn't included in the 'standard' beacons -
// I've included the flags, since certain apps seem to expect them
#define DEVNAME "DMP%02X%02X"
#define DEVNAME_LEN 8  // incl term null
#define DISPLAY_NAME "Switch"

// Bluetooth stack configuration
#define MAX_CONNECTIONS EMBER_AF_PLUGIN_BLE_MAX_CONNECTIONS

enum {
  DMP_CONNECT_SWITCH_COMMAND_ID_DATA = 0,
};

typedef enum {
  DEMO_EVT_NONE                     = 0x00,
  DEMO_EVT_BOOTED                   = 0x01,
  DEMO_EVT_BLUETOOTH_CONNECTED      = 0x02,
  DEMO_EVT_BLUETOOTH_DISCONNECTED   = 0x03,
  DEMO_EVT_CONNECT_JOINING          = 0x04,
  DEMO_EVT_CONNECT_PAIRED           = 0x05,
  DEMO_EVT_CONNECT_KEY_EXCHANGE     = 0x06,
  DEMO_EVT_CONNECT_DISCONNECTED     = 0x07,
  DEMO_EVT_LIGHT_CHANGED_BLUETOOTH  = 0x08,
  DEMO_EVT_LIGHT_CHANGED_CONNECT    = 0x09,
  DEMO_EVT_INDICATION               = 0x0A,
  DEMO_EVT_INDICATION_SUCCESSFUL    = 0x0B,
  DEMO_EVT_INDICATION_FAILED        = 0x0C,
  DEMO_EVT_BUTTON0_PRESSED          = 0x0D,
  DEMO_EVT_BUTTON1_PRESSED          = 0x0E,
  DEMO_EVT_BUTTON1_LONG_PRESS       = 0x0F,
} DemoMsg;

typedef enum {
  demoLightOff = 0,
  demoLightOn  = 1
} DemoLight;

typedef enum {
  demoLightDirectionBluetooth   = 0,
  demoLightDirectionProprietary = 1,
  demoLightDirectionButton      = 2,
  demoLightDirectionInvalid     = 3
} DemoLightDirection;

typedef enum {
  demoTmrIndicate   = 0x02
} DemoTmr;

typedef enum {
  DEMO_STATE_INIT       = 0x00,
  DEMO_STATE_READY      = 0x01
} DemoState;

typedef struct {
  bool inUse;
  uint8_t handle;
  bd_addr address;
} BleConn;

struct responseData_t{
  uint8_t flagsLen;          /**< Length of the Flags field. */
  uint8_t flagsType;         /**< Type of the Flags field. */
  uint8_t flags;             /**< Flags field. */
  uint8_t shortNameLen;      /**< Length of Shortened Local Name. */
  uint8_t shortNameType;     /**< Shortened Local Name. */
  uint8_t shortName[DEVNAME_LEN]; /**< Shortened Local Name. */
  uint8_t uuidLength;        /**< Length of UUID. */
  uint8_t uuidType;          /**< Type of UUID. */
  uint8_t uuid[UUID_LEN];    /**< 128-bit UUID. */
};

typedef struct {
  uint8_t addr[8];
} DemoLightSrcAddr;

typedef struct {
  DemoState state;
  DemoLight light;
  enum gatt_client_config_flag lightInd;
  DemoLightDirection direction;
  enum gatt_client_config_flag directionInd;
  DemoLightSrcAddr srcAddr;
  enum gatt_client_config_flag srcAddrInd;
  uint8_t connBluetoothInUse;
  uint8_t connConnectInUse;
  bool indicationOngoing;
  bool indicationPending;
  DemoLightSrcAddr ownAddr;
} Demo;

typedef uint8_t dmpConnectSwitchCommandId;
EmberEventControl lcdDirectionDisplayEventControl;

static OS_Q    demoQueue;
static uint8_t boot_to_dfu = 0;
volatile bool lightStateToggle = false;
static BleConn bleConn[MAX_CONNECTIONS];
static uint8_t retryScan = SCAN_RETRY_ATTEMPT;
static volatile connectMsg conMsgFlag = false;
static uint8_t bestChannel = 0XFF;
static bool bestChannelFound = FALSE;
static int8_t txPower = DMP_CONNECT_SWITCH_TX_POWER;
static EmberMessageOptions txOptions = EMBER_OPTIONS_ACK_REQUESTED;
static EmberKeyData securityKey = DMP_CONNECT_SWITCH_SECURITY_KEY;
uint8_t bluetooth_stack_heap[DEFAULT_BLUETOOTH_HEAP(MAX_CONNECTIONS)];

static char* switchText[5] = { "No Nwk", "SCAN", "SCAN", "KEY_EXCH", "PAN:0000" };

connect_t connect = {
  .state = CONNECT_STATE_INVALID,
  .networkParams.panId = DMP_CONNECT_SWITCH_PAN_ID,
  .destNodeId = 0xFFFF,
  .sourceNodeId = 0xFFFF,
};

// iBeacon structure and data
static struct {
  uint8_t flagsLen;     /* Length of the Flags field. */
  uint8_t flagsType;    /* Type of the Flags field. */
  uint8_t flags;        /* Flags field. */
  uint8_t mandataLen;   /* Length of the Manufacturer Data field. */
  uint8_t mandataType;  /* Type of the Manufacturer Data field. */
  uint8_t compId[2];    /* Company ID field. */
  uint8_t beacType[2];  /* Beacon Type field. */
  uint8_t uuid[16];     /* 128-bit Universally Unique Identifier (UUID). The UUID is an identifier for the company using the beacon*/
  uint8_t majNum[2];    /* Beacon major number. Used to group related beacons. */
  uint8_t minNum[2];    /* Beacon minor number. Used to specify individual beacons within a group.*/
  uint8_t txPower;      /* The Beacon's measured RSSI at 1 meter distance in dBm. See the iBeacon specification for measurement guidelines. */
}
iBeaconData
  = {
  /* Flag bits - See Bluetooth 4.0 Core Specification , Volume 3, Appendix C, 18.1 for more details on flags. */
  2,  /* length  */
  0x01, /* type */
  0x04 | 0x02, /* Flags: LE General Discoverable Mode, BR/EDR is disabled. */

  /* Manufacturer specific data */
  26,  /* length of field*/
  0xFF, /* type of field */

  /* The first two data octets shall contain a company identifier code from
   * the Assigned Numbers - Company Identifiers document */
  { UINT16_TO_BYTES(0x004C) },

  /* Beacon type */
  /* 0x0215 is iBeacon */
  { UINT16_TO_BYTE1(0x0215), UINT16_TO_BYTE0(0x0215) },

  /* 128 bit / 16 byte UUID - generated specially for the DMP Demo */
  { 0x00, 0x47, 0xe7, 0x0a, 0x5d, 0xc1, 0x47, 0x25, 0x87, 0x99, 0x83, 0x05, 0x44, 0xae, 0x04, 0xf6 },

  /* Beacon major number */
  { UINT16_TO_BYTE1(IBEACON_MAJOR_NUM), UINT16_TO_BYTE0(IBEACON_MAJOR_NUM) },

  /* Beacon minor number  - not used for this application*/
  { UINT16_TO_BYTE1(0), UINT16_TO_BYTE0(0) },

  /* The Beacon's measured RSSI at 1 meter distance in dBm */
  /* 0xC3 is -61dBm */
  // TBD: check?
  0xC3
  };

static struct {
  uint8_t flagsLen;          /**< Length of the Flags field. */
  uint8_t flagsType;         /**< Type of the Flags field. */
  uint8_t flags;             /**< Flags field. */
  uint8_t serLen;            /**< Length of Complete list of 16-bit Service UUIDs. */
  uint8_t serType;           /**< Complete list of 16-bit Service UUIDs. */
  uint8_t serviceList[2];    /**< Complete list of 16-bit Service UUIDs. */
  uint8_t serDataLength;     /**< Length of Service Data. */
  uint8_t serDataType;       /**< Type of Service Data. */
  uint8_t uuid[2];           /**< 16-bit Eddystone UUID. */
  uint8_t frameType;         /**< Frame type. */
  uint8_t txPower;           /**< The Beacon's measured RSSI at 0 meter distance in dBm. */
  uint8_t urlPrefix;         /**< URL prefix type. */
  uint8_t url[10];           /**< URL. */
} eddystone_data = {
  /* Flag bits - See Bluetooth 4.0 Core Specification , Volume 3, Appendix C, 18.1 for more details on flags. */
  2,  /* length  */
  0x01, /* type */
  0x04 | 0x02, /* Flags: LE General Discoverable Mode, BR/EDR is disabled. */
  /* Service field length */
  0x03,
  /* Service field type */
  0x03,
  /* 16-bit Eddystone UUID */
  { UINT16_TO_BYTES(0xFEAA) },
  /* Eddystone-TLM Frame length */
  0x10,
  /* Service Data data type value */
  0x16,
  /* 16-bit Eddystone UUID */
  { UINT16_TO_BYTES(0xFEAA) },
  /* Eddystone-URL Frame type */
  0x10,
  /* Tx power */
  0x00,
  /* URL prefix - standard */
  0x00,
  /* URL */
  { 's', 'i', 'l', 'a', 'b', 's', '.', 'c', 'o', 'm' }
};

static struct responseData_t responseData = {
  2,  /* length (incl type) */
  0x01, /* type */
  0x04 | 0x02, /* Flags: LE General Discoverable Mode, BR/EDR is disabled. */
  DEVNAME_LEN + 1,        // length of local name (incl type)
  0x08,               // shortened local name
  { 'D', 'M', '0', '0', ':', '0', '0' },
  UUID_LEN + 1,           // length of UUID data (incl type)
  0x06,               // incomplete list of service UUID's
  // custom service UUID for silabs light in little-endian format
  // 62792313-adf2-4fc9-97 4d- fa b9 dd f2 62 2c
  { 0x2C, 0x62, 0xF2, 0xDD, 0xb9, 0xFA, 0x4D, 0x97, 0xC9, 0x4F, 0xF2, 0xAD, 0x13, 0x23, 0x79, 0x62 }
};

static Demo demo = {
  .state = DEMO_STATE_INIT,
  .light = demoLightOff,
  .lightInd = gatt_disable,
  .direction = demoLightDirectionButton,
  .directionInd = gatt_disable,
  .srcAddr = { { 0, 0, 0, 0, 0, 0, 0, 0 } },
  .srcAddrInd = gatt_disable,
  .connBluetoothInUse = 0,
  .connConnectInUse = 0,
  .indicationOngoing = false,
  .indicationPending = false,
  .ownAddr = { { 0, 0, 0, 0, 0, 0, 0, 0 } }
};
static bool performKeyExchange = false;
static void writeLightState(uint8_t connection,
                            uint8array *writeValue);
static void writeOtaControl(uint8_t connection,
                            uint8array *writeValue);
static void readLightState(uint8_t connection);
static void readTriggerSource(uint8_t connection);
static void readSourceAddress(uint8_t connection);
static void notifyLightState(void);
static void startSleepySlaveDevice(void);
static void updateDisplay(uint8_t protocolDirection);

/** GATT Server Attribute User Read Configuration.
 *  Structure to register handler functions to user read events. */
typedef struct {
  uint16 charId; /**< ID of the Characteristic. */
  void (*fctn)(uint8_t connection); /**< Handler function. */
} AppCfgGattServerUserReadRequest_t;

/** GATT Server Attribute Value Write Configuration.
 *  Structure to register handler functions to characteristic write events. */
typedef struct {
  uint16 charId; /**< ID of the Characteristic. */
  /**< Handler function. */
  void (*fctn)(uint8_t connection, uint8array * writeValue);
} AppCfgGattServerUserWriteRequest_t;

static const AppCfgGattServerUserReadRequest_t appCfgGattServerUserReadRequest[] =
{
  { gattdb_light_state, readLightState },
  { gattdb_trigger_source, readTriggerSource },
  { gattdb_source_address, readSourceAddress },
  { 0, NULL }
};

static const AppCfgGattServerUserWriteRequest_t appCfgGattServerUserWriteRequest[] =
{
  { gattdb_light_state, writeLightState },
  { gattdb_ota_control, writeOtaControl },
  { 0, NULL }
};

size_t appCfgGattServerUserWriteRequestSize = COUNTOF(appCfgGattServerUserWriteRequest) - 1;
size_t appCfgGattServerUserReadRequestSize = COUNTOF(appCfgGattServerUserReadRequest) - 1;

static void appUiLedOff(void)
{
  BSP_LedClear(0);
  BSP_LedClear(1);
}

static void appUiLedOn(void)
{
  BSP_LedSet(0);
  BSP_LedSet(1);
}

static connectMsg getConnectFlag()
{
  return conMsgFlag;
}
static void setConnectFlag(connectMsg flag)
{
  conMsgFlag = flag;
}
static void clearConnectFlag()
{
  conMsgFlag = CONNECT_MSG_INVALID;
}

static inline DemoMsg demoQueuePend(RTOS_ERR* err)
{
  DemoMsg demoMsg;
  OS_MSG_SIZE demoMsgSize;
  demoMsg = (DemoMsg)OSQPend((OS_Q*       )&demoQueue,
                             (OS_TICK     ) 0,
                             (OS_OPT      ) OS_OPT_PEND_BLOCKING,
                             (OS_MSG_SIZE*)&demoMsgSize,
                             (CPU_TS*     ) DEF_NULL,
                             (RTOS_ERR*   ) err);
  APP_RTOS_ASSERT_DBG((RTOS_ERR_CODE_GET(*err) == RTOS_ERR_NONE), 1);
  return demoMsg;
}

static inline void demoQueuePost(DemoMsg msg,
                                 RTOS_ERR* err)
{
  OSQPost((OS_Q*      )&demoQueue,
          (void*      ) msg,
          (OS_MSG_SIZE) sizeof(void*),
          (OS_OPT     ) OS_OPT_POST_FIFO,
          (RTOS_ERR*  ) err);

  APP_RTOS_ASSERT_DBG((RTOS_ERR_CODE_GET(*err) == RTOS_ERR_NONE), 1);
}

connect_t * getConnectParam(void)
{
  return &connect;
}
static void resetConnectParam(void)
{
  MEMSET(&connect, 0, sizeof(connect_t));
  connect.state = CONNECT_STATE_INVALID;
  connect.networkParams.panId = DMP_CONNECT_SWITCH_PAN_ID;
  connect.destNodeId = 0xFFFF;
  connect.sourceNodeId = 0xFFFF;
  connect.networkParams.radioChannel = CHANNEL_NUMBER_START;
  performKeyExchange = true;
}

static void saveConnectParam(uint8_t connectState)
{
  connect.networkParams.panId = emberGetPanId();
  connect.networkParams.radioChannel = emberGetRadioChannel();
  connect.networkParams.radioTxPower = emberGetRadioPower();
  connect.sourceNodeId = emberGetNodeId();
  memcpy(connect.sourceEui64, emberGetEui64(), EUI64_SIZE);
  connect.destNodeId = emberGetParentId();
  connect.state = connectState;
}

//------------------------------------------------------------------------------
// Callbacks

// This callback is called when the application starts and can be used to
// perform any additional initialization required at system startup.
void emberAfMainInitCallback(void)
{
  emberAfCorePrintln("Powered UP");
  emberAfCorePrintln("\n%p>", EMBER_AF_DEVICE_NAME);

  EmberStatus initStatus = emberNetworkInit();
  dmpUiInit();
  EmberNetworkStatus status = emberNetworkState();
  if (status == EMBER_NO_NETWORK) {
    dmpUiDisplayHelp(true);
    if (initStatus == EMBER_NOT_JOINED) {
      performKeyExchange = true;
    }
  }
}

// This callback is called in each iteration of the main application loop and
// can be used to perform periodic functions.
void emberAfMainTickCallback(void)
{
  RTOS_ERR err;
  connectMsg conMsg;
  switch (connect.state) {
    case CONNECT_STATE_PAIRED:
    {
      uint8_t light;
      conMsg = getConnectFlag();
      clearConnectFlag();
      if (conMsg == CONNECT_JOINED) {
        emberAfCorePrintln("[C] CONNECT MSG JOINED\n");
#if APP_SECURITY_ENABLE
        // Key exchange for the first time joining
        if (performKeyExchange) {
          connect.state = CONNECT_STATE_KEY_EXCHANGE;
          setConnectFlag(CONNECT_KEY_EXCHANGE);
        } else {
          // No key exchange on reboots in secure mode
          connect.sourceNodeId = emberGetNodeId();
          demoQueuePost(DEMO_EVT_CONNECT_PAIRED, &err);
        }
#else
        connect.sourceNodeId = emberGetNodeId();
        demoQueuePost(DEMO_EVT_CONNECT_PAIRED, &err);
#endif // APP_SECURITY_ENABLE
      } else if (conMsg == CONNECT_LIGHT_SET_STATUS_REQ) {
        emberAfCorePrintln("[C] CONNECT_LIGHT_SET_STATUS_REQ\n");
        light = demo.light;
        emberSendMsgReq(connect.sourceNodeId, false, COMMAND_RELAY_SET_REQ, &light, 1);
      } else if (conMsg == CONNECT_LIGHT_READ_STATUS_REQ) {
        emberAfCorePrintln("[C] CONNECT_LIGHT_READ_STATUS_REQ\n");
        emberSendMsgReq(connect.sourceNodeId, false, COMMAND_RELAY_READ_REQ, NULL, 0);
      } else if (conMsg == CONNECT_READ_EUI64_REQ) {
        emberAfCorePrintln("[C] CONNECT_READ_EUI64_REQ\n");
        emberSendMsgReq(connect.sourceNodeId, false, COMMAND_READ_EUI64_REQ, NULL, 0);
      }
    }
    break;
#if APP_SECURITY_ENABLE
    case CONNECT_STATE_KEY_EXCHANGE:
    {
      conMsg = getConnectFlag();
      clearConnectFlag();
      if (conMsg == CONNECT_KEY_EXCHANGE) {
        emberSendPairInitRequest(emberGetEui64(), true);
        demoQueuePost(DEMO_EVT_CONNECT_KEY_EXCHANGE, &err);
      } else if (conMsg == CONNECT_KEY_EXCHANGE_SUCCESS) {
        connect.sourceNodeId = emberGetNodeId();
        connect.state = CONNECT_STATE_PAIRED;
        demoQueuePost(DEMO_EVT_CONNECT_PAIRED, &err);
      } else if (conMsg == CONNECT_KEY_EXCHANGE_FAIL) {
        connect.state = CONNECT_STATE_INVALID;
      }
    }
    break;
#endif //APP_SECURITY_ENABLE
    case CONNECT_STATE_JOIN:
    {
      conMsg = getConnectFlag();
      clearConnectFlag();
      if (conMsg == CONNECT_RETRY_ATTEMPT_FAIL) {
        emberAfCorePrintln("[C] CONNECT MSG JOIN FAILED\n");
        connect.state = CONNECT_STATE_INVALID;
        updateDisplay(0);
      } else if (conMsg == CONNECT_JOIN_FAIL) {
        emberAfCorePrintln("[C] CONNECT MSG JOIN FAIL\n");
        connect.state = CONNECT_STATE_INVALID;
      }
    }
    break;
    case CONNECT_STATE_INVALID:
    {
      conMsg = getConnectFlag();
      clearConnectFlag();
      if (conMsg == CONNECT_BUTTON1_PRESSED) {
        startSleepySlaveDevice();
        connect.state = CONNECT_STATE_JOIN;
        setConnectFlag(CONNECT_STATE_JOIN);
        demoQueuePost(DEMO_EVT_CONNECT_JOINING, &err);
      } else if (conMsg == CONNECT_BUTTON1_LONG_PRESS) {
        emberAfCorePrintln("[C] CONNECT MSG BUTTON 1 PRESSED Reset..leave network\n");
        emberResetNetworkState();
      }
    }
    break;
    default:
      break;
  }
}

void emberAfIncomingMessageCallback(EmberIncomingMessage *message)
{
  if (message->length < DMP_CONNECT_SWITCH_MINIMUM_LENGTH
      || (emberFetchLowHighInt16u(message->payload
                                  + DMP_CONNECT_SWITCH_PROTOCOL_ID_OFFSET)
          != DMP_CONNECT_SWITCH_PROTOCOL_ID)) {
    return;
  }

  switch (message->payload[DMP_CONNECT_SWITCH_COMMAND_ID_OFFSET]) {
    case DMP_CONNECT_SWITCH_COMMAND_ID_DATA:
    {
      uint8_t i;
      emberAfCorePrint("RX: Data from 0x%2x:", message->source);
      for (i = DMP_CONNECT_SWITCH_DATA_OFFSET; i < message->length; i++) {
        emberAfCorePrint(" %x", message->payload[i]);
      }
      emberAfCorePrintln("");
      emebrAppReceiveEventHandler(message->source,
                                  &message->payload[DMP_CONNECT_SWITCH_DATA_OFFSET],
                                  message->length - DMP_CONNECT_SWITCH_DATA_OFFSET);
      break;
    }
  }
}

void emberAfMessageSentCallback(EmberStatus status,
                                EmberOutgoingMessage *message)
{
  emebrAppTransmitEventHandler(status);
}

void emberAfStackStatusCallback(EmberStatus status)
{
  RTOS_ERR err;
  switch (status) {
    case EMBER_NETWORK_UP:
      emberAfCorePrintln("Network up");
      saveConnectParam(CONNECT_STATE_PAIRED);
      setConnectFlag(CONNECT_JOINED);
      // If the node joined as a sleepy slave start short polling.
      emberAfPluginPollEnableShortPolling(emberGetNodeType()
                                          == EMBER_STAR_SLEEPY_END_DEVICE);
      break;
    case EMBER_NETWORK_DOWN:
      resetConnectParam();
      demoQueuePost(DEMO_EVT_CONNECT_DISCONNECTED, &err);
      emberAfCorePrintln("Network down");
      dmpUiDisplayHelp(true);
      break;
    case EMBER_JOIN_FAILED:
      emberAfCorePrintln("Join failed");
      setConnectFlag(CONNECT_JOIN_FAIL);
      break;
    default:
      emberAfCorePrintln("Stack status: 0x%x", status);
      setConnectFlag(CONNECT_JOIN_FAIL);
      break;
  }
}

void emberAfChildJoinCallback(EmberNodeType nodeType,
                              EmberNodeId nodeId)
{
  emberAfCorePrintln("Slave 0x%2x joined as %s", nodeId,
                     (nodeType == EMBER_STAR_END_DEVICE)
                     ? "end device"
                     : "sleepy");
}

#if defined(EMBER_AF_PLUGIN_MICRIUM_RTOS) && defined(EMBER_AF_PLUGIN_MICRIUM_RTOS_APP_TASK1)

#define DEV_ID_STR_LEN                    9u
char devIdStr[DEV_ID_STR_LEN];

static void updateDisplay(uint8_t protocolDirection)
{
  char tempbuff[9] = "\0";
  dmpUiClearMainScreen((uint8_t*)DISPLAY_NAME, true, true);
  dmpUiDisplayLight(demo.light);
  if (demo.connBluetoothInUse) {
    dmpUiDisplayProtocol(DMP_UI_PROTOCOL2, true);
  } else {
    dmpUiDisplayProtocol(DMP_UI_PROTOCOL2, false);
  }
  if (demo.connConnectInUse) {
    dmpUiDisplayProtocol(DMP_UI_PROTOCOL1, true);
  } else {
    dmpUiDisplayProtocol(DMP_UI_PROTOCOL1, false);
  }
  dmpUiDisplayId(DMP_UI_PROTOCOL2, (uint8_t*)devIdStr);

  if (connect.state == CONNECT_STATE_PAIRED) {
    snprintf(tempbuff, 9, "PAN:%04X", connect.networkParams.panId);
    dmpUiDisplayId(DMP_UI_PROTOCOL1, (uint8_t*)tempbuff);
  } else {
    dmpUiDisplayId(DMP_UI_PROTOCOL1, (uint8_t*)switchText[connect.state]);
  }

  if (protocolDirection == 0) {
    dmpUiClearDirection(DMP_UI_DIRECTION_PROT1);
    dmpUiClearDirection(DMP_UI_DIRECTION_PROT2);
  } else if (protocolDirection == 1) {
    dmpUiDisplayDirection(DMP_UI_DIRECTION_PROT1);
  } else if (protocolDirection == 2) {
    dmpUiDisplayDirection(DMP_UI_DIRECTION_PROT2);
  }
}

void emberAfPluginMicriumRtosAppTask1InitCallback(void)
{
  RTOS_ERR err;
  snprintf(devIdStr, DEV_ID_STR_LEN, "DMP:%04X", *((uint16*)demo.ownAddr.addr));
  OSQCreate((OS_Q     *)&demoQueue,
            (CPU_CHAR *)"Demo Queue",
            (OS_MSG_QTY) 32,
            (RTOS_ERR *)&err);
  APP_RTOS_ASSERT_DBG((RTOS_ERR_CODE_GET(err) == RTOS_ERR_NONE), 1);

  demo.state = DEMO_STATE_INIT;
}

void emberAfPluginMicriumRtosAppTask1MainLoopCallback(void *p_arg)
{
  RTOS_ERR err;
  DemoMsg demoMsg;
  snprintf(devIdStr, DEV_ID_STR_LEN, "DMP:%04X", *((uint16*)demo.ownAddr.addr));
  bool bluetoothInd = false;
  demo.state = DEMO_STATE_INIT;
  while (true) {
    // pending on demo message queue
    demoMsg = demoQueuePend(&err);
    APP_RTOS_ASSERT_DBG((RTOS_ERR_CODE_GET(err) == RTOS_ERR_NONE), 1);
    switch (demo.state) {
      case DEMO_STATE_INIT:
        switch (demoMsg) {
          // bluetooth booted
          case DEMO_EVT_BOOTED:
            snprintf(devIdStr, DEV_ID_STR_LEN, "DMP:%04X", *((uint16*)demo.ownAddr.addr));
            // initialise demo variables:
            // make it look like the last trigger source of the light was a button press
            demo.connBluetoothInUse = 0;
            demo.connConnectInUse = 0;
            demo.indicationOngoing = false;
            demo.direction = demoLightDirectionButton;
            Mem_Copy((void*)demo.srcAddr.addr,
                     (void*)demo.ownAddr.addr,
                     sizeof(demo.srcAddr.addr));
            demo.state = DEMO_STATE_READY;
            break;

          default:
            break;
        }
        break;

      case DEMO_STATE_READY:
        switch (demoMsg) {
          case DEMO_EVT_BLUETOOTH_CONNECTED:
            emberAfCorePrintln("  [E] DEMO_EVT_BLUETOOTH_CONNECTED\n");
            demo.indicationOngoing = false;
            if (demo.connBluetoothInUse < MAX_CONNECTIONS) {
              demo.connBluetoothInUse += 1;
            }
            updateDisplay(0);
            emberAfCorePrintln("active BLE connection=%d", demo.connBluetoothInUse);
            break;

          case DEMO_EVT_BLUETOOTH_DISCONNECTED:
            emberAfCorePrintln("  [E] DEMO_EVT_BLUETOOTH_DISCONNECTED\n");
            demo.indicationOngoing = false;
            if (demo.connBluetoothInUse > 0) {
              demo.connBluetoothInUse -= 1;
            }
            updateDisplay(0);
            emberAfCorePrintln("active BLE connection=%d", demo.connBluetoothInUse);
            break;
          case DEMO_EVT_CONNECT_KEY_EXCHANGE:
            updateDisplay(0);
            break;
          case DEMO_EVT_CONNECT_PAIRED:
            emberAfCorePrintln("  [E] DEMO_EVT_CONNECT_PAIRED\n");
            demo.connConnectInUse = 1;
            setConnectFlag(CONNECT_READ_EUI64_REQ);
            updateDisplay(0);
            break;

          case DEMO_EVT_CONNECT_JOINING:
            emberAfCorePrintln("  [E] DEMO_EVT_CONNECT_JOINING\n");
            demo.connConnectInUse = 0;
            updateDisplay(0);
            break;
          case DEMO_EVT_CONNECT_DISCONNECTED:
            emberAfCorePrintln("  [E] DEMO_EVT_CONNECT_DISCONNECTED\n");
            demo.connConnectInUse = 0;
            break;
          case DEMO_EVT_LIGHT_CHANGED_BLUETOOTH:
            emberAfCorePrintln("  [E] DEMO_EVT_LIGHT_CHANGED_BLUETOOTH\n");
            if (demoLightOff == demo.light) {
              appUiLedOff();
            } else {
              appUiLedOn();
            }
            updateDisplay(2);
            setConnectFlag(CONNECT_LIGHT_SET_STATUS_REQ);
            emberEventControlSetDelayMS(lcdDirectionDisplayEventControl, 500);
            APP_RTOS_ASSERT_DBG((RTOS_ERR_CODE_GET(err) == RTOS_ERR_NONE), 1);
            demoQueuePost(DEMO_EVT_INDICATION, &err);
            break;

          case DEMO_EVT_LIGHT_CHANGED_CONNECT:
            emberAfCorePrintln("  [E] DEMO_EVT_LIGHT_CHANGED_CONNECT\n");
            if (demoLightOff == demo.light) {
              appUiLedOff();
            } else {
              appUiLedOn();
            }
            demo.direction = demoLightDirectionButton;
            Mem_Copy((void*)demo.srcAddr.addr,
                     (void*)connect.destEui64,
                     sizeof(demo.srcAddr.addr));
            updateDisplay(0);
            APP_RTOS_ASSERT_DBG((RTOS_ERR_CODE_GET(err) == RTOS_ERR_NONE), 1);
            demoQueuePost(DEMO_EVT_INDICATION, &err);
            break;
          case DEMO_EVT_BUTTON0_PRESSED:
            emberAfCorePrintln("  [E] DEMO_EVT_BUTTON0_PRESSED\n");
            if (demoLightOff == demo.light) {
              demo.light = demoLightOn;
              appUiLedOn();
            } else {
              demo.light = demoLightOff;
              appUiLedOff();
            }
            demo.direction = demoLightDirectionProprietary;
            Mem_Copy((void*)demo.srcAddr.addr,
                     (void*)connect.sourceEui64,
                     sizeof(demo.srcAddr.addr));
            updateDisplay(1);
            setConnectFlag(CONNECT_LIGHT_SET_STATUS_REQ);
            emberEventControlSetDelayMS(lcdDirectionDisplayEventControl, 500);
            demoQueuePost(DEMO_EVT_INDICATION, &err);
            break;

          case DEMO_EVT_BUTTON1_PRESSED:
            emberAfCorePrintln("  [E] DEMO_EVT_BUTTON1_PRESSED\n");
            setConnectFlag(CONNECT_BUTTON1_PRESSED);
            break;
          case DEMO_EVT_BUTTON1_LONG_PRESS:
            emberAfCorePrintln("  [E] DEMO_EVT_BUTTON1_LONG_PRESS\n");
            setConnectFlag(CONNECT_BUTTON1_LONG_PRESS);
            break;
          case DEMO_EVT_INDICATION:
            bluetoothInd = demo.connBluetoothInUse
                           && (demo.lightInd == gatt_indication
                               || demo.directionInd == gatt_indication
                               || demo.srcAddrInd == gatt_indication);
            // no ongoing indication, free to start sending one out
            if (!demo.indicationOngoing) {
              if (bluetoothInd || demo.connConnectInUse) {
                demo.indicationOngoing = true;
              }
              // send indication on BLE side
              if (bluetoothInd) {
                notifyLightState();
              }
              // send indication on proprietary side
              // there is no protocol on proprietary side; BLE side transmission is the slower
              // send out proprietary packets at maximum the same rate as BLE indications
              if (demo.connConnectInUse) {
                if (!bluetoothInd) {
                  demoQueuePost(DEMO_EVT_INDICATION_SUCCESSFUL, &err);
                }
              }
              // ongoing indication; set pending flag
            } else {
              demo.indicationPending = true;
            }
            break;

          case DEMO_EVT_INDICATION_SUCCESSFUL:
          case DEMO_EVT_INDICATION_FAILED:
            emberAfCorePrintln("  [E] DEMO_EVT_INDICATION_SUCCESSFUL\n");
            demo.indicationOngoing = false;
            if (demo.indicationPending) {
              demo.indicationPending = false;
              demoQueuePost(DEMO_EVT_INDICATION, &err);
            }
            break;

          default:
            APP_RTOS_ASSERT_DBG(false, DEF_NULL);
            break;
        }
        break;

      // error
      default:
        APP_RTOS_ASSERT_DBG(false, DEF_NULL);
        break;
    }
  }
}

#endif // EMBER_AF_PLUGIN_MICRIUM_RTOS && EMBER_AF_PLUGIN_MICRIUM_RTOS_APP_TASK1

//------------------------------------------------------------------------------
// Static functions

static EmberStatus send(EmberNodeId nodeId,
                        dmpConnectSwitchCommandId commandId,
                        uint8_t *buffer,
                        uint8_t bufferLength)
{
  uint8_t message[EMBER_MAX_UNSECURED_APPLICATION_PAYLOAD_LENGTH];
  EmberMessageLength messageLength = 0;

  if ((bufferLength + DMP_CONNECT_SWITCH_HEADER_LENGTH)
      > ((txOptions & EMBER_OPTIONS_SECURITY_ENABLED)
         ? EMBER_MAX_SECURED_APPLICATION_PAYLOAD_LENGTH
         : EMBER_MAX_UNSECURED_APPLICATION_PAYLOAD_LENGTH)) {
    return EMBER_MESSAGE_TOO_LONG;
  }
  emberStoreLowHighInt16u(message + messageLength, DMP_CONNECT_SWITCH_PROTOCOL_ID);
  messageLength += 2;
  message[messageLength++] = commandId;
  if (bufferLength != 0) {
    MEMCOPY(message + messageLength, buffer, bufferLength);
    messageLength += bufferLength;
  }

  return emberMessageSend(nodeId,
                          0, // endpoint
                          0, // messageTag
                          messageLength,
                          message,
                          txOptions);
}

static void startSleepySlaveDevice(void)
{
  // Initialize the security key to the default key prior to joining the
  // network.
  retryScan = SCAN_RETRY_ATTEMPT;
  emberAfCorePrintln("startSleepySlaveDevice");
  emberSetSecurityKey(&securityKey);
  emberStartActiveScan(connect.networkParams.radioChannel);
}

static void pairSleepySlaveDevice(void)
{
  EmberStatus status;
  EmberNetworkParameters *params;
  srand(halCommonGetInt16uMillisecondTick());
  connect.sourceNodeId = rand();
  params = &((getConnectParam())->networkParams);
  params->radioChannel = connect.networkParams.radioChannel;
  params->radioTxPower = txPower;

  status = emberJoinNetworkExtended(EMBER_STAR_SLEEPY_END_DEVICE,
                                    connect.sourceNodeId,
                                    params);
  if (status != EMBER_SUCCESS) {
    emberAfCorePrintln("Join as sleepy failed, 0x%x", status);
  }
}

/** @brief Incoming Beacon Extended
 *
 * This function is called when a node is performing an active scan and a beacon
 * is received.
 *
 * @param panId                Ver.: always
 * @param source               Ver.: always
 * @param rssi                 Ver.: always
 * @param permitJoining        Ver.: always
 * @param beaconFieldsLength   Ver.: always
 * @param beaconFields         Ver.: always
 * @param beaconPayloadLength  Ver.: always
 * @param beaconPayload        Ver.: always
 */
void emberAfIncomingBeaconExtendedCallback(EmberPanId panId,
                                           EmberMacAddress *source,
                                           int8_t rssi,
                                           bool permitJoining,
                                           uint8_t beaconFieldsLength,
                                           uint8_t *beaconFields,
                                           uint8_t beaconPayloadLength,
                                           uint8_t *beaconPayload)
{
  emberAfCorePrintln("emberAfIncomingBeaconExtendedCallback = %d", connect.networkParams.radioChannel);
  if (permitJoining) {
    bestChannelFound = TRUE;
    bestChannel = connect.networkParams.radioChannel;
    emberAfCorePrintln("bestChannel = %d source->addr.shortAddress = 0x%2x", bestChannel, source->addr.shortAddress);
    connect.networkParams.panId = panId;
    connect.destNodeId = source->addr.shortAddress;
  }
}
/** @brief Active Scan Complete
 *
 * This function is called when a node has completed an active scan.
 */
void emberAfActiveScanCompleteCallback(void)
{
  emberAfCorePrintln("emberAfActiveScanCompleteCallback = %d", connect.networkParams.radioChannel);
  if (!bestChannelFound) {
    connect.networkParams.radioChannel++;
    if (connect.networkParams.radioChannel > CHANNEL_NUMBER_END) {
      connect.networkParams.radioChannel = CHANNEL_NUMBER_START;
      --retryScan;
      if (!retryScan) {
        setConnectFlag(CONNECT_RETRY_ATTEMPT_FAIL);
        return;
      }
    }
    emberStartActiveScan(connect.networkParams.radioChannel);
  } else {
    connect.networkParams.radioChannel = bestChannel;
    bestChannelFound = FALSE;
    bestChannel = 0;
    pairSleepySlaveDevice();
  }
}

void setSecurityKeyCommand(void)
{
  EmberKeyData key;
  emberCopyKeyArgument(0, &key);

  if (emberSetSecurityKey(&key) == EMBER_SUCCESS) {
    emberAfCorePrint("Security key set { ");
    emberAfCorePrintBuffer(key.contents, EMBER_ENCRYPTION_KEY_SIZE, TRUE);
    emberAfCorePrintln("}");
  } else {
    emberAfCorePrintln("Security key set failed");
  }
}

EmberStatus connectDataSend(void *payload,
                            uint8_t payloadLength)
{
  EmberStatus status;

  status = send(connect.destNodeId,
                DMP_CONNECT_SWITCH_COMMAND_ID_DATA,
                payload,
                payloadLength);

  emberAfCorePrint("TX: Data to 0x%2x:{", connect.destNodeId);
  emberAfCorePrintBuffer(payload, payloadLength, TRUE);
  emberAfCorePrintln("}: status=0x%x", status);
  return status;
}

static void printBleAddress(uint8_t *address)
{
  emberAfCorePrint("[%x %x %x %x %x %x]", address[5], address[4], address[3],
                   address[2], address[1], address[0]);
}

static void bleConnectionInfoTablePrintEntry(uint8_t index)
{
  assert(index < EMBER_AF_PLUGIN_BLE_MAX_CONNECTIONS
         && bleConn[index].inUse);
  emberAfCorePrintln("**** Connection Info index[%d]****", index);
  emberAfCorePrintln("connection handle 0x%x",
                     bleConn[index].handle);
  emberAfCorePrint("remote address: ");
  printBleAddress(bleConn[index].address.addr);
  emberAfCorePrintln("");
  emberAfCorePrintln("*************************");
}

// return false on error
static bool bleAddConn(uint8_t handle,
                       bd_addr* address)
{
  for (uint8_t i = 0; i < MAX_CONNECTIONS; i++) {
    if (bleConn[i].inUse == false) {
      bleConn[i].handle = handle;
      Mem_Copy((void*)&bleConn[i].address,
               (void*)address,
               sizeof(bleConn[i].address));
      bleConn[i].inUse = true;
      bleConnectionInfoTablePrintEntry(i);
      return true;
    }
  }
  return false;
}

static bool bleRemoveConn(uint8_t handle)
{
  for (uint8_t i = 0; i < MAX_CONNECTIONS; i++) {
    if (bleConn[i].handle == handle) {
      bleConn[i].handle = 0;
      Mem_Set((void*)&bleConn[i].address.addr, 0, sizeof(bleConn[i].address.addr));
      bleConn[i].inUse = false;
      return true;
    }
  }
  return false;
}

static bd_addr* bleGetAddr(uint8_t handle)
{
  for (uint8_t i = 0; i < MAX_CONNECTIONS; i++) {
    if (bleConn[i].handle == handle) {
      return &(bleConn[i].address);
    }
  }
  return (bd_addr*)DEF_NULL;
}

static void beaconAdvertisements(void)
{
  static uint8_t *advData;
  static uint8_t advDataLen;

  advData = (uint8_t*)&iBeaconData;
  advDataLen = sizeof(iBeaconData);
  // Set custom advertising data.
  gecko_cmd_le_gap_bt5_set_adv_data(1, 0, advDataLen, advData);
  gecko_cmd_le_gap_set_advertise_timing(1, 160, 160, 0, 0);
  // Use le_gap_non_resolvable type, configuration flag is 1 (bit 0).
  gecko_cmd_le_gap_set_advertise_configuration(1, 1);
  gecko_cmd_le_gap_start_advertising(1, le_gap_user_data, le_gap_non_connectable);

  advData = (uint8_t*)&eddystone_data;
  advDataLen = sizeof(eddystone_data);
  // Set custom advertising data.
  gecko_cmd_le_gap_bt5_set_adv_data(2, 0, advDataLen, advData);
  gecko_cmd_le_gap_set_advertise_timing(2, 160, 160, 0, 0);
  // Use le_gap_non_resolvable type, configuration flag is 1 (bit 0).
  gecko_cmd_le_gap_set_advertise_configuration(2, 1);
  gecko_cmd_le_gap_start_advertising(2, le_gap_user_data, le_gap_non_connectable);
}

static void enableBleAdvertisements(void)
{
  // set transmit power to 0 dBm.
  gecko_cmd_system_set_tx_power(0);
  // Create the device name based on the 16-bit device ID.
  uint16_t devId;
  devId = *((uint16*)demo.ownAddr.addr);

  // Copy to the local GATT database - this will be used by the BLE stack
  // to put the local device name into the advertisements, but only if we are
  // using default advertisements
  static char devName[DEVNAME_LEN];
  snprintf(devName, DEVNAME_LEN, DEVNAME, devId >> 8, devId & 0xff);
  gecko_cmd_gatt_server_write_attribute_value(gattdb_device_name,
                                              0,
                                              strlen(devName),
                                              (uint8_t *)devName);
  // Copy the shortened device name to the response data, overwriting
  // the default device name which is set at compile time
  Mem_Copy(((uint8_t*)&responseData) + 5, devName, 8);
  // Set the response data
  gecko_cmd_le_gap_bt5_set_adv_data(0, 0, sizeof(responseData), (uint8_t*)&responseData);
  // Set nominal 100ms advertising interval, so we just get
  // a single beacon of each type
  gecko_cmd_le_gap_set_advertise_timing(0, 160, 160, 0, 0);
  gecko_cmd_le_gap_set_advertise_report_scan_request(0, 1);
  /* Start advertising in user mode and enable connections, if we are
   * not already connected */
  if (demo.connBluetoothInUse > MAX_CONNECTIONS) {
    gecko_cmd_le_gap_start_advertising(0, le_gap_user_data, le_gap_non_connectable);
  } else {
    gecko_cmd_le_gap_start_advertising(0, le_gap_user_data, le_gap_connectable_scannable);
  }
  beaconAdvertisements();
}

/** @brief
 *
 * This function is called at init time. The following fields will be
 * overwritten by the framework:
 *  - max_connections
 *  - heap
 *  - heap_size
 */
void emberAfPluginBleGetConfigCallback(gecko_configuration_t* config)
{
  // Change the BLE configuration here if needed*/
  config->bluetooth.max_advertisers = 3;
  config->bluetooth.max_periodic_sync = 4;   //!< Maximum number of synhronous advertisers to support. Default is 0, none supported
}

static void writeLightState(uint8_t connection,
                            uint8array *writeValue)
{
  bd_addr *addrPoi = bleGetAddr(connection);
  APP_RTOS_ASSERT_DBG(addrPoi, DEF_NULL);
  demo.light = (DemoLight) writeValue->data[0],
  demo.direction = demoLightDirectionBluetooth;
  Mem_Copy(demo.srcAddr.addr, (void const*)addrPoi->addr, sizeof(bd_addr));
  /* Send response to write request */
  gecko_cmd_gatt_server_send_user_write_response(connection,
                                                 LIGHT_STATE_GATTDB,
                                                 ES_WRITE_OK);
}

static void writeOtaControl(uint8_t connection,
                            uint8array *writeValue)
{
  // Event related to OTA upgrading
  // Check if the user-type OTA Control Characteristic was written.
  // If ota_control was written, boot the device into Device Firmware
  // Upgrade (DFU) mode.
  // Set flag to enter to OTA mode.
  boot_to_dfu = 1;
  // Send response to Write Request.
  gecko_cmd_gatt_server_send_user_write_response(connection,
                                                 OTA_CONTROL_GATTDB,
                                                 bg_err_success);
  // Close connection to enter to DFU OTA mode.
  gecko_cmd_le_connection_close(connection);
}
static void readLightState(uint8_t connection)
{
  /* Send response to read request */
  gecko_cmd_gatt_server_send_user_read_response(connection,
                                                gattdb_light_state,
                                                0,
                                                (uint8_t)sizeof(demo.light),
                                                (uint8_t*)&demo.light);
}

static void readTriggerSource(uint8_t connection)
{
  /* Send response to read request */
  gecko_cmd_gatt_server_send_user_read_response(connection,
                                                gattdb_trigger_source,
                                                0,
                                                (uint8_t)sizeof(demo.direction),
                                                (uint8_t*)&demo.direction);
}

static void readSourceAddress(uint8_t connection)
{
  /* Send response to read request */
  gecko_cmd_gatt_server_send_user_read_response(connection,
                                                gattdb_source_address,
                                                0,
                                                (uint8_t)sizeof(demo.srcAddr.addr),
                                                (uint8_t*)demo.srcAddr.addr);
}

static void notifyLightState(void)

{
  if (demo.lightInd == gatt_indication) {
    /* Send notification/indication data */
    for (int i = 0; i < EMBER_AF_PLUGIN_BLE_MAX_CONNECTIONS; i++) {
      if (bleConn[i].inUse
          && bleConn[i].handle) {
        gecko_cmd_hardware_set_soft_timer(TIMER_S_2_TIMERTICK(1), demoTmrIndicate, true);
        gecko_cmd_gatt_server_send_characteristic_notification(bleConn[i].handle,
                                                               LIGHT_STATE_GATTDB,
                                                               (uint8_t)sizeof(demo.light),
                                                               (uint8_t*)&demo.light);
      }
    }
  }
}

static void notifyDirection(uint8_t connection)
{
  if (demo.directionInd == gatt_indication) {
    /* Send notification/indication data */
    gecko_cmd_hardware_set_soft_timer(TIMER_S_2_TIMERTICK(1), demoTmrIndicate, true);
    gecko_cmd_gatt_server_send_characteristic_notification(connection,
                                                           TRIGGER_SOURCE_GATTDB,
                                                           (uint8_t)sizeof(demo.direction),
                                                           (uint8_t*)&demo.direction);
  }
}

static void notifySourceAddr(uint8_t connection)
{
  if (demo.srcAddrInd == gatt_indication) {
    /* Send notification/indication data */
    gecko_cmd_hardware_set_soft_timer(TIMER_S_2_TIMERTICK(1), demoTmrIndicate, true);
    gecko_cmd_gatt_server_send_characteristic_notification(connection,
                                                           SOURCE_ADDRESS_GATTDB,
                                                           (uint8_t)sizeof(demo.srcAddr.addr),
                                                           (uint8_t*)demo.srcAddr.addr);
  }
}

/** @brief
 *
 * This function is called from the BLE stack to notify the application of a
 * stack event.
 */
void emberAfPluginBleEventCallback(struct gecko_cmd_packet* evt)
{
  RTOS_ERR err;
  switch (BGLIB_MSG_ID(evt->header)) {
    case gecko_evt_system_boot_id: {
      struct gecko_msg_system_get_bt_address_rsp_t *get_address_rsp;

      // own address: Bluetooth device address
      Mem_Set((void*)demo.ownAddr.addr, 0, sizeof(demo.ownAddr.addr));
      Mem_Copy((void*)demo.ownAddr.addr,
               (void*)gecko_cmd_system_get_bt_address()->address.addr,
               sizeof(demo.ownAddr.addr));
      demo.indicationOngoing = false;
      demoQueuePost(DEMO_EVT_BOOTED, &err);
      get_address_rsp = gecko_cmd_system_get_bt_address();
      emberAfCorePrint("BLE address: ");
      printBleAddress(get_address_rsp->address.addr);
      emberAfCorePrintln("");
      // start advertising
      enableBleAdvertisements();
    }
    break;
    case gecko_evt_le_connection_opened_id: {
      emberAfCorePrintln("gecko_evt_le_connection_opened_id \n");
      struct gecko_msg_le_connection_opened_evt_t *conn_evt =
        (struct gecko_msg_le_connection_opened_evt_t*) &(evt->data);
      if (demo.connBluetoothInUse >= MAX_CONNECTIONS) {
        emberAfCorePrintln("MAX active BLE connections");
        //assert(index < 0xFF);
      } else {
        APP_RTOS_ASSERT_DBG(bleAddConn(conn_evt->connection,
                                       (bd_addr*)&bluetooth_evt->data.evt_le_connection_opened.address),
                            DEF_NULL);
        enableBleAdvertisements();
        emberAfCorePrintln("BLE connection opened");
        demoQueuePost(DEMO_EVT_BLUETOOTH_CONNECTED, &err);
      }
    }
    break;
    case gecko_evt_le_connection_phy_status_id: {
      // indicate the PHY that has been selected
      emberAfCorePrintln("now using the %dMPHY\r\n",
                         evt->data.evt_le_connection_phy_status.phy);
    }
    break;
    case gecko_evt_le_connection_closed_id: {
      struct gecko_msg_le_connection_closed_evt_t *conn_evt =
        (struct gecko_msg_le_connection_closed_evt_t*) &(evt->data);

      // Check if need to boot to DFU mode.
      if (boot_to_dfu) {
        // Enter to DFU OTA mode.
        gecko_cmd_system_reset(2);
      } else {
        APP_RTOS_ASSERT_DBG(bleRemoveConn(conn_evt->connection),
                            DEF_NULL);
      }
      demoQueuePost(DEMO_EVT_BLUETOOTH_DISCONNECTED, &err);
      // restart advertising, set connectable
      enableBleAdvertisements();
      emberAfCorePrintln(
        "BLE connection closed, handle=0x%x, reason=0x%2x", conn_evt->connection, conn_evt->reason);
    }
    break;
    /* This event indicates that a remote GATT client is attempting to write a value of an
     * attribute in to the local GATT database, where the attribute was defined in the GATT
     * XML firmware configuration file to have type="user".  */
    case gecko_evt_gatt_server_user_write_request_id:
      // light state write
      emberAfCorePrintln("gecko_evt_gatt_server_user_write_request_id");
      for (int i = 0; i < appCfgGattServerUserWriteRequestSize; i++) {
        if ((appCfgGattServerUserWriteRequest[i].charId
             == evt->data.evt_gatt_server_user_write_request.characteristic)
            && (appCfgGattServerUserWriteRequest[i].fctn)) {
          appCfgGattServerUserWriteRequest[i].fctn(
            evt->data.evt_gatt_server_user_write_request.connection,
            &(evt->data.evt_gatt_server_attribute_value.value));
        }
      }
      if (LIGHT_STATE_GATTDB == bluetooth_evt->data.evt_gatt_server_user_write_request.characteristic) {
        demoQueuePost(DEMO_EVT_LIGHT_CHANGED_BLUETOOTH, &err);
      }
      break;

    /* This event indicates that a remote GATT client is attempting to read a value of an
     *  attribute from the local GATT database, where the attribute was defined in the GATT
     *  XML firmware configuration file to have type="user". */

    case gecko_evt_gatt_server_user_read_request_id:
      for (int i = 0; i < appCfgGattServerUserReadRequestSize; i++) {
        if ((appCfgGattServerUserReadRequest[i].charId
             == evt->data.evt_gatt_server_user_read_request.characteristic)
            && (appCfgGattServerUserReadRequest[i].fctn)) {
          appCfgGattServerUserReadRequest[i].fctn(
            evt->data.evt_gatt_server_user_read_request.connection);
        }
      }
      break;
    case gecko_evt_le_gap_scan_request_id:
      break;

    case gecko_evt_le_gap_scan_response_id: {
      struct gecko_msg_le_gap_scan_response_evt_t *scan_evt =
        (struct gecko_msg_le_gap_scan_response_evt_t*) &(evt->data);
      emberAfCorePrint("Scan response, address type=0x%x, address: ",
                       scan_evt->address_type);
      printBleAddress(scan_evt->address.addr);
      emberAfCorePrintln("");
    }
    break;

    case gecko_evt_sm_list_bonding_entry_id: {
      struct gecko_msg_sm_list_bonding_entry_evt_t * bonding_entry_evt =
        (struct gecko_msg_sm_list_bonding_entry_evt_t*) &(evt->data);
      emberAfCorePrint("Bonding handle=0x%x, address type=0x%x, address: ",
                       bonding_entry_evt->bonding,
                       bonding_entry_evt->address_type);
      emberAfCorePrintln("");
    }
    break;
    case gecko_evt_gatt_service_id: {
      struct gecko_msg_gatt_service_evt_t* service_evt =
        (struct gecko_msg_gatt_service_evt_t*) &(evt->data);
      uint8_t i;
      emberAfCorePrintln(
        "GATT service, conn_handle=0x%x, service_handle=0x%4x",
        service_evt->connection, service_evt->service);
      emberAfCorePrint("UUID=[");
      for (i = 0; i < service_evt->uuid.len; i++) {
        emberAfCorePrint("0x%x ", service_evt->uuid.data[i]);
      }
      emberAfCorePrintln("]");
    }
    break;
    case gecko_evt_gatt_characteristic_id: {
      struct gecko_msg_gatt_characteristic_evt_t* char_evt =
        (struct gecko_msg_gatt_characteristic_evt_t*) &(evt->data);
      uint8_t i;
      emberAfCorePrintln(
        "GATT characteristic, conn_handle=0x%x, char_handle=0x%2x, properties=0x%x",
        char_evt->connection,
        char_evt->characteristic,
        char_evt->properties);
      emberAfCorePrint("UUID=[");
      for (i = 0; i < char_evt->uuid.len; i++) {
        emberAfCorePrint("0x%x ", char_evt->uuid.data[i]);
      }
      emberAfCorePrintln("]");
    }
    break;
    /* This event indicates either that a local Client Characteristic Configuration descriptor
     * has been changed by the remote GATT client, or that a confirmation from the remote GATT
     * client was received upon a successful reception of the indication. */
    case gecko_evt_gatt_server_characteristic_status_id:
      if (LIGHT_STATE_GATTDB == bluetooth_evt->data.evt_gatt_server_characteristic_status.characteristic) {
        // confirmation of indication received from remote GATT client
        if (gatt_server_confirmation == (enum gatt_server_characteristic_status_flag)bluetooth_evt->data.evt_gatt_server_characteristic_status.status_flags) {
          gecko_cmd_hardware_set_soft_timer(0, demoTmrIndicate, false);
          notifyDirection(bluetooth_evt->data.evt_gatt_server_characteristic_status.connection);
          // client characteristic configuration changed by remote GATT client
        } else if (gatt_server_client_config == (enum gatt_server_characteristic_status_flag)bluetooth_evt->data.evt_gatt_server_characteristic_status.status_flags) {
          demo.lightInd = (enum gatt_client_config_flag)bluetooth_evt->data.evt_gatt_server_characteristic_status.client_config_flags;
          // unhandled event
        } else {
          APP_RTOS_ASSERT_DBG(false, DEF_NULL);
        }
      } else if (TRIGGER_SOURCE_GATTDB == bluetooth_evt->data.evt_gatt_server_characteristic_status.characteristic) {
        // confirmation of indication received from GATT client
        if (gatt_server_confirmation == (enum gatt_server_characteristic_status_flag)bluetooth_evt->data.evt_gatt_server_characteristic_status.status_flags) {
          gecko_cmd_hardware_set_soft_timer(0, demoTmrIndicate, false);
          notifySourceAddr(bluetooth_evt->data.evt_gatt_server_characteristic_status.connection);
          // client characteristic configuration changed by remote GATT client
        } else if (gatt_server_client_config == (enum gatt_server_characteristic_status_flag)bluetooth_evt->data.evt_gatt_server_characteristic_status.status_flags) {
          demo.directionInd = (enum gatt_client_config_flag)bluetooth_evt->data.evt_gatt_server_characteristic_status.client_config_flags;
          // unhandled event
        } else {
          APP_RTOS_ASSERT_DBG(false, DEF_NULL);
        }
      } else if (SOURCE_ADDRESS_GATTDB == bluetooth_evt->data.evt_gatt_server_characteristic_status.characteristic) {
        // confirmation of indication received from GATT client
        if (gatt_server_confirmation == (enum gatt_server_characteristic_status_flag)bluetooth_evt->data.evt_gatt_server_characteristic_status.status_flags) {
          gecko_cmd_hardware_set_soft_timer(0, demoTmrIndicate, false);
          demoQueuePost(DEMO_EVT_INDICATION_SUCCESSFUL, &err);
          // client characteristic configuration changed by remote GATT client
        } else if (gatt_server_client_config == (enum gatt_server_characteristic_status_flag)bluetooth_evt->data.evt_gatt_server_characteristic_status.status_flags) {
          demo.srcAddrInd = (enum gatt_client_config_flag)bluetooth_evt->data.evt_gatt_server_characteristic_status.client_config_flags;
          // unhandled event
        } else {
          APP_RTOS_ASSERT_DBG(false, DEF_NULL);
        }
      } else {
      }
      break;
    case gecko_evt_gatt_characteristic_value_id: {
      struct gecko_msg_gatt_characteristic_value_evt_t* char_evt =
        (struct gecko_msg_gatt_characteristic_value_evt_t*) &(evt->data);
      uint8_t i;

      if (char_evt->att_opcode == gatt_handle_value_indication) {
        gecko_cmd_gatt_send_characteristic_confirmation(
          char_evt->connection);
      }
      emberAfCorePrintln(
        "GATT (client) characteristic value, handle=0x%x, characteristic=0x%2x, att_op_code=0x%x",
        char_evt->connection,
        char_evt->characteristic,
        char_evt->att_opcode);
      emberAfCorePrint("value=[");
      for (i = 0; i < char_evt->value.len; i++) {
        emberAfCorePrint("0x%x ", char_evt->value.data[i]);
      }
      emberAfCorePrintln("]");
    }
    break;
    case gecko_evt_gatt_server_attribute_value_id: {
      struct gecko_msg_gatt_server_attribute_value_evt_t* attr_evt =
        (struct gecko_msg_gatt_server_attribute_value_evt_t*) &(evt->data);
      uint8_t i;
      emberAfCorePrintln(
        "GATT (server) attribute value, handle=0x%x, attribute=0x%2x, att_op_code=0x%x",
        attr_evt->connection,
        attr_evt->attribute,
        attr_evt->att_opcode);
      emberAfCorePrint("value=[");
      for (i = 0; i < attr_evt->value.len; i++) {
        emberAfCorePrint("0x%x ", attr_evt->value.data[i]);
      }
      emberAfCorePrintln("]");
    }
    break;
    case gecko_evt_le_connection_parameters_id: {
      struct gecko_msg_le_connection_parameters_evt_t* param_evt =
        (struct gecko_msg_le_connection_parameters_evt_t*) &(evt->data);
      emberAfCorePrintln(
        "BLE connection parameters are updated, handle=0x%x, interval=0x%2x, latency=0x%2x, timeout=0x%2x, security=0x%x, txsize=0x%2x",
        param_evt->connection,
        param_evt->interval,
        param_evt->latency,
        param_evt->timeout,
        param_evt->security_mode,
        param_evt->txsize);
    }
    break;
    case gecko_evt_gatt_procedure_completed_id: {
      struct gecko_msg_gatt_procedure_completed_evt_t* proc_comp_evt =
        (struct gecko_msg_gatt_procedure_completed_evt_t*) &(evt->data);
      emberAfCorePrintln("BLE procedure completed, handle=0x%x, result=0x%2x",
                         proc_comp_evt->connection, proc_comp_evt->result);
    }
    break;
    /* Software Timer event */
    case gecko_evt_hardware_soft_timer_id:
      if (demoTmrIndicate == bluetooth_evt->data.evt_hardware_soft_timer.handle) {
        demoQueuePost(DEMO_EVT_INDICATION_FAILED, &err);
        emberAfCorePrintln("  DEMO_EVT_INDICATION_FAILED\n");
      } else {
        APP_RTOS_ASSERT_DBG(false, DEF_NULL);
      }
      break;
  }
}

void halButtonIsr(uint8_t button,
                  uint8_t state)
{
  RTOS_ERR err;
  static uint16_t buttonPressTime;
  uint16_t currentTime = 0;
  if (state == BUTTON_PRESSED) {
    if (button == BUTTON1) {
      buttonPressTime = halCommonGetInt16uMillisecondTick();
    }
  } else if (state == BUTTON_RELEASED) {
    if (button == BUTTON0) {
      demoQueuePost(DEMO_EVT_BUTTON0_PRESSED, &err);
    }
    if (button == BUTTON1) {
      currentTime = halCommonGetInt16uMillisecondTick();
      if ((currentTime - buttonPressTime) > BUTTON_LONG_PRESS_TIME_MSEC) {
        resetConnectParam();
        demoQueuePost(DEMO_EVT_BUTTON1_LONG_PRESS, &err);
      } else {
        demoQueuePost(DEMO_EVT_BUTTON1_PRESSED, &err);
      }
    }
  }
}

void readConnectLightState(uint8_t *payload)
{
  *payload = demo.light;
}

void writeConnectLightState(CommandType_t cmd,
                            uint8_t *payload,
                            uint8_t payloadLen)
{
  if (cmd == COMMAND_RELAY_READ_RESP
      || cmd == COMMAND_RELAY_SET_RESP) {
    RTOS_ERR  err;
    demo.light = payload[0];
    demoQueuePost(DEMO_EVT_LIGHT_CHANGED_CONNECT, &err);
  }
}

void setEUI64FromResponse(uint8_t * payload, uint8_t payloadLen)
{
  if (payloadLen == EUI64_SIZE) {
    MEMCOPY(connect.destEui64, payload, payloadLen);
  }
  setConnectFlag(CONNECT_LIGHT_READ_STATUS_REQ);
}

void lcdDirectionDisplayEventHandler(void)
{
  emberEventControlSetInactive(lcdDirectionDisplayEventControl);
  updateDisplay(0);
}
// Called from the mbedTLS key exchange, sets the new key in the
// Connect security context
void emberAppMbedtlsKeyExchangeNegotiatedKey(uint8_t *key,
                                             uint8_t keylength)
{
  EmberKeyData mbedTLSSecurityKey;
  MEMCOPY(mbedTLSSecurityKey.contents,
          key,
          EMBER_ENCRYPTION_KEY_SIZE);
  emberSetSecurityKey(&mbedTLSSecurityKey);
}
// Called from the mbedTLS key exchange, sets the node state to conclude
// key exchange phase.
void emberAppMbedtlsKeyExchangeCallback(void)
{
  setConnectFlag(CONNECT_KEY_EXCHANGE_SUCCESS);
}

//------------------------------------------------------------------------------
// CLI commands
void startSlaveCommand(void)
{
  EmberStatus status;
  EmberNetworkParameters params;

  // Initialize the security key to the default key prior to joining the
  // network.
  emberSetSecurityKey(&securityKey);

  params.panId = DMP_CONNECT_SWITCH_PAN_ID;
  params.radioChannel = emberUnsignedCommandArgument(0);
  params.radioTxPower = txPower;

  status = emberJoinNetworkExtended(EMBER_STAR_END_DEVICE,
                                    DMP_CONNECT_SWITCH_SLAVE_ID,
                                    &params);

  if (status != EMBER_SUCCESS) {
    emberAfCorePrintln("Join failed, 0x%x", status);
  }
}

void startSleepySlaveCommand(void)
{
  EmberStatus status;
  EmberNetworkParameters params;

  // Initialize the security key to the default key prior to joining the
  // network.
  emberSetSecurityKey(&securityKey);

  params.panId = DMP_CONNECT_SWITCH_PAN_ID;
  params.radioChannel = emberUnsignedCommandArgument(0);
  params.radioTxPower = txPower;

  status = emberJoinNetworkExtended(EMBER_STAR_SLEEPY_END_DEVICE,
                                    DMP_CONNECT_SWITCH_SLAVE_ID,
                                    &params);

  if (status != EMBER_SUCCESS) {
    emberAfCorePrintln("Join as sleepy failed, 0x%x", status);
  }
}

void setTxOptionsCommand(void)
{
  txOptions = emberSignedCommandArgument(0);
  emberAfCorePrintln("TX options set: MAC acks %s, security %s, priority %s",
                     ((txOptions & EMBER_OPTIONS_ACK_REQUESTED) ? "enabled" : "disabled"),
                     ((txOptions & EMBER_OPTIONS_SECURITY_ENABLED) ? "enabled" : "disabled"),
                     ((txOptions & EMBER_OPTIONS_HIGH_PRIORITY) ? "enabled" : "disabled"));
}

void setTxPowerCommand(void)
{
  txPower = emberSignedCommandArgument(0);

  if (emberSetRadioPower(txPower) == EMBER_SUCCESS) {
    emberAfCorePrintln("TX power set: %d", (int8_t)emberGetRadioPower());
  } else {
    emberAfCorePrintln("TX power set failed");
  }
}

void dataCommand(void)
{
  EmberStatus status;
  uint8_t length;
  uint8_t *contents = emberStringCommandArgument(0, &length);
  EmberNodeId destination = (emberGetNodeType() == EMBER_STAR_COORDINATOR)
                            ? DMP_CONNECT_SWITCH_SLAVE_ID
                            : DMP_CONNECT_SWITCH_MASTER_ID;

  status = send(destination,
                DMP_CONNECT_SWITCH_COMMAND_ID_DATA,
                contents,
                length);

  emberAfCorePrint("TX: Data to 0x%2x:{", destination);
  emberAfCorePrintBuffer(contents, length, TRUE);
  emberAfCorePrintln("}: status=0x%x", status);
}

void infoCommand(void)
{
  emberAfCorePrintln("%p:", EMBER_AF_DEVICE_NAME);
  emberAfCorePrintln("  network state: 0x%x", emberNetworkState());
  emberAfCorePrintln("      node type: 0x%x", emberGetNodeType());
  emberAfCorePrintln("          eui64: >%x%x%x%x%x%x%x%x",
                     emberGetEui64()[7],
                     emberGetEui64()[6],
                     emberGetEui64()[5],
                     emberGetEui64()[4],
                     emberGetEui64()[3],
                     emberGetEui64()[2],
                     emberGetEui64()[1],
                     emberGetEui64()[0]);
  emberAfCorePrintln("        node id: 0x%2x", emberGetNodeId());
  emberAfCorePrintln("         pan id: 0x%2x", emberGetPanId());
  emberAfCorePrintln("        channel: %d", (uint16_t)emberGetRadioChannel());
  emberAfCorePrintln("          power: %d", (int16_t)emberGetRadioPower());
  emberAfCorePrintln("        node id: 0x%2x", emberGetParentId());
}
