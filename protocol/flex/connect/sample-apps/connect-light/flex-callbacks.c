/***************************************************************************//**
 * @brief Connect light sample application.
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
#include "application/app-join.h"
#include "token.h"
#include "flex-token.h"
#include "rail_config.h"

#define CONNECT_LIGHT_TX_POWER         0
#define CONNECT_LIGHT_PAN_ID           0xDEED
#define CONNECT_LIGHT_PROTOCOL_ID      0xC00E
#define CONNECT_LIGHT_MASTER_ID        EMBER_COORDINATOR_ADDRESS
#define CONNECT_LIGHT_SLAVE_ID         (EMBER_COORDINATOR_ADDRESS + 1)

#define CONNECT_LIGHT_PROTOCOL_ID_OFFSET  0
#define CONNECT_LIGHT_COMMAND_ID_OFFSET   2
#define CONNECT_LIGHT_DATA_OFFSET         3
#define CONNECT_LIGHT_COMMAND_OFFSET       3
#define CONNECT_LIGHT_MINIMUM_LENGTH      CONNECT_LIGHT_DATA_OFFSET
#define CONNECT_LIGHT_HEADER_LENGTH       CONNECT_LIGHT_DATA_OFFSET

#define CONNECT_LIGHT_ENERGY_SCAN_SAMPLE  10
#define CHANNELMAP_RSSI_THRESHOLD           -70
#define NETWORK_UP_LED                       BOARDLED0
#define BUTTON_LONG_PRESS_TIME_MSEC          3000
#define PJOIN_TIMEOUT                        180

#define CHANNEL_NUMBER_START                 (channelConfigs[0]->configs->channelNumberStart)
#define CHANNEL_NUMBER_END                   (channelConfigs[0]->configs->channelNumberEnd)
#define CONNECT_LIGHT_SECURITY_KEY        { 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, \
                                            0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, \
                                            0xAA, 0xAA, 0xAA, 0xAA }
#define DISPLAY_NAME "Light"

enum {
  CONNECT_LIGHT_COMMAND_ID_DATA = 0,
};

typedef enum {
  demoLightOff = 0,
  demoLightOn  = 1
} DemoLight;

typedef struct {
  uint8_t addr[8];
} DemoLightSrcAddr;

typedef struct {
  DemoLight light;
  DemoLightSrcAddr ownAddr;
  bool connConnectInUse;
} Demo;

typedef uint8_t connectLightCommandId;
EmberEventControl lcdDirectionDisplayEventControl;
EmberEventControl permitJointTimeoutEventControl;

static char* lightText[5] = { "No Nwk", "No Nwk", "PJOIN", "KEY_EXCH", "PAN:0000" };
static int8_t txPower = CONNECT_LIGHT_TX_POWER;
static volatile connectMsg conMsgFlag = false;
static EmberMessageOptions txOptions = EMBER_OPTIONS_ACK_REQUESTED;
static EmberKeyData securityKey = CONNECT_LIGHT_SECURITY_KEY;

Demo demo = { 0 };

connect_t connect = {
  .state = CONNECT_STATE_INVALID,
  .networkParams.panId = CONNECT_LIGHT_PAN_ID,
};

// Clear child node id
void emConnectClearNodeId(void)
{
  connect.destNodeId = 0;
  halCommonSetToken(TOKEN_CONNECT_DEVICE_NODE_ID, &connect.destNodeId);
}
// Set child node id
void emConnectSetNodeId(void)
{
  halCommonSetToken(TOKEN_CONNECT_DEVICE_NODE_ID, &connect.destNodeId);
}

// Get child node id
EmberNodeId emConnectGetNodeId(void)
{
  halCommonGetToken(&connect.destNodeId, TOKEN_CONNECT_DEVICE_NODE_ID);
  return connect.destNodeId;
}

// Read child node Id from flash
void emConnectInitNodeId(void)
{
  halCommonGetToken(&connect.destNodeId, TOKEN_CONNECT_DEVICE_NODE_ID);
}

connect_t * getConnectParam(void)
{
  emConnectGetNodeId();
  return &connect;
}
static void resetConnectParam(void)
{
  MEMSET(&connect, 0, sizeof(connect_t));
  connect.state = CONNECT_STATE_INVALID;
  connect.networkParams.panId = CONNECT_LIGHT_PAN_ID;
  connect.destNodeId = 0xFFFF;
  connect.sourceNodeId = 0xFFFF;
  emConnectClearNodeId();
  connect.networkParams.radioChannel = CHANNEL_NUMBER_START;
}

static void saveConnectParam(uint8_t connectState,
                             EmberNodeId childNodeId)
{
  connect.networkParams.panId = emberGetPanId();
  connect.networkParams.radioChannel = emberGetRadioChannel();
  connect.networkParams.radioTxPower = emberGetRadioPower();
  connect.sourceNodeId = emberGetNodeId();
  if (childNodeId) {
    connect.destNodeId = childNodeId;
    emConnectSetNodeId();
  }
  memcpy(connect.sourceEui64, emberGetEui64(), EUI64_SIZE);
  connect.state = connectState;
}

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

static void updateDisplay(bool protocolDirection)
{
  char tempbuff[9] = "\0";
  dmpUiClearMainScreen((uint8_t*)DISPLAY_NAME, true, false);
  dmpUiDisplayLight(demo.light);
  if (connect.state == CONNECT_STATE_PAIRED) {
    snprintf(tempbuff, 9, "PAN:%04X", connect.networkParams.panId);
    dmpUiDisplayId(DMP_UI_PROTOCOL1, (uint8_t*)tempbuff);
  } else {
    dmpUiDisplayId(DMP_UI_PROTOCOL1, (uint8_t*)lightText[connect.state]);
  }
  if (demo.connConnectInUse) {
    dmpUiDisplayProtocol(DMP_UI_PROTOCOL1, true);
  } else {
    dmpUiDisplayProtocol(DMP_UI_PROTOCOL1, false);
  }
  if (!protocolDirection) {
    dmpUiClearDirection(DMP_UI_DIRECTION_PROT1);
  } else {
    dmpUiDisplayDirection(DMP_UI_DIRECTION_PROT1);
  }
}

//------------------------------------------------------------------------------
// Callbacks

// This callback is called when the application starts and can be used to
// perform any additional initialization required at system startup.
void emberAfMainInitCallback(void)
{
  emberAfCorePrintln("Powered UP");
  emberAfCorePrintln("\n%p>", EMBER_AF_DEVICE_NAME);

  emberNetworkInit();
  emConnectInitNodeId();
  dmpUiInit();
  EmberNetworkStatus status = emberNetworkState();
  if (status == EMBER_NO_NETWORK) {
    dmpUiDisplayHelp(true);
  }
}

static void permitJoinMasterDevice(uint8_t duration)
{
  EmberStatus status;
  status = emberPermitJoining(duration);
  if (status != EMBER_SUCCESS) {
    emberAfCorePrintln("Permit Join failed, 0x%x", status);
  }
}

static void startMasterDevice(void)
{
  EmberStatus status;
  EmberNetworkParameters params;

  // Initialize the security key to the default key prior to forming the
  // network.
  emberSetSecurityKey(&securityKey);
  params.panId = connect.networkParams.panId;
  params.radioChannel = connect.networkParams.radioChannel;
  params.radioTxPower = txPower;
  status = emberFormNetwork(&params);

  if (status != EMBER_SUCCESS) {
    emberAfCorePrintln("Form failed, 0x%x", status);
  } else {
    permitJoinMasterDevice(0xFF);
  }
}

void emberAfEnergyScanCompleteCallback(int8_t mean,
                                       int8_t min,
                                       int8_t max,
                                       uint16_t variance)
{
  static uint8_t bestChannel = 0;
  static int8_t lowestRssi = 0;
  EmberStatus status;

  emberAfCorePrintln("Energy scan complete, mean=%d min=%d max=%d var=%d",
                     mean, min, max, variance);
  if ( CHANNEL_NUMBER_START == connect.networkParams.radioChannel) {
    bestChannel = connect.networkParams.radioChannel;
    lowestRssi = mean;
  } else {
    emberAfCorePrintln("energy scanning: radio channel %d",
                       connect.networkParams.radioChannel);
    if (mean < CHANNELMAP_RSSI_THRESHOLD) {
      // update best channel
      bestChannel = (lowestRssi > mean) ? connect.networkParams.radioChannel : bestChannel;
      // Update lowest rssi;
      lowestRssi = (lowestRssi > mean) ? mean : lowestRssi;
    }
    emberAfCorePrintln(" best channel %d: lowestRssi %d", bestChannel, lowestRssi);
    // Check if looped through all the channels ?
    if (connect.networkParams.radioChannel >= CHANNEL_NUMBER_END) {
      // set the best channel as operational channel
      connect.networkParams.radioChannel = bestChannel;
      emberAfCorePrintln("energy scanning: best channel %d", bestChannel);
      bestChannel = 0;
      lowestRssi = 0;
      startMasterDevice();
      return;
    }
  }
  connect.networkParams.radioChannel++;
  status = emberStartEnergyScan(connect.networkParams.radioChannel,
                                CONNECT_LIGHT_ENERGY_SCAN_SAMPLE);

  if (status != EMBER_SUCCESS) {
    emberAfCorePrintln("Start energy scanning failed, status=0x%x", status);
  } else {
    emberAfCorePrintln("Start energy scanning: channel %d, samples %d",
                       connect.networkParams.radioChannel, CONNECT_LIGHT_ENERGY_SCAN_SAMPLE);
  }
}

static void startEnergyScan(void)
{
  EmberStatus status;
  status = emberStartEnergyScan(connect.networkParams.radioChannel,
                                CONNECT_LIGHT_ENERGY_SCAN_SAMPLE);

  if (status != EMBER_SUCCESS) {
    emberAfCorePrintln("Start energy scanning failed, status=0x%x", status);
  } else {
    emberAfCorePrintln("Start energy scanning: channel %d, samples %d",
                       connect.networkParams.radioChannel,
                       CONNECT_LIGHT_ENERGY_SCAN_SAMPLE);
  }
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

// This callback is called in each iteration of the main application loop and
// can be used to perform periodic functions.
void emberAfMainTickCallback(void)
{
  static bool permitJoinSet = false;
  connectMsg flag = CONNECT_MSG_INVALID;

  switch (connect.state) {
    case CONNECT_STATE_PAIRED:
    {
      flag = getConnectFlag();
      if (flag == CONNECT_PERMIT_JOIN) {
        if (permitJoinSet) {
          permitJoinSet = false;
          clearConnectFlag();
          permitJoinMasterDevice(0);
          emberEventControlSetInactive(permitJointTimeoutEventControl);
          updateDisplay(0);
        } else {
          permitJoinSet = true;
          emberSetSecurityKey(&securityKey);
          permitJoinMasterDevice(PJOIN_TIMEOUT);
          emberEventControlSetDelayMS(permitJointTimeoutEventControl, (PJOIN_TIMEOUT * 1000));
          clearConnectFlag();
          connect.state = CONNECT_STATE_JOIN;
          updateDisplay(0);
        }
      } else if (flag == CONNECT_LIGHT_SET_STATUS_REQ) {
        emberAfCorePrintln("[C] STATE PAIRED: MSG CONNECT_LIGHT_SET_STATUS_REQ");
        clearConnectFlag();
        if (demoLightOff == demo.light) {
          appUiLedOff();
        } else {
          appUiLedOn();
        }
        updateDisplay(1);
        emberEventControlSetDelayMS(lcdDirectionDisplayEventControl, 500);
      } else if (flag == CONNECT_BUTTON0_PRESSED) {
        emberAfCorePrintln("[C] STATE PAIRED: MSG CONNECT_BUTTON0_PRESSED");
        clearConnectFlag();
        if (demoLightOff == demo.light) {
          demo.light = demoLightOn;
          appUiLedOn();
        } else {
          demo.light = demoLightOff;
          appUiLedOff();
        }
        uint8_t light = demo.light;
        emberSendMsgResp(connect.sourceNodeId, false, COMMAND_RELAY_SET_RESP, &light, 1);
        updateDisplay(0);
      }
    }
    break;
    case CONNECT_STATE_JOIN:
    {
      flag = getConnectFlag();
      if (flag == CONNECT_JOINED) {
        clearConnectFlag();
        connect.state = CONNECT_STATE_PAIRED;
        emberAfCorePrintln("[C] STATE PJOIN: MSG CONNECT_PAIRED");
        demo.connConnectInUse = true;
        updateDisplay(0);
      } else if (flag == CONNECT_PERMIT_JOIN) {
        if (permitJoinSet) {
          permitJoinSet = false;
          clearConnectFlag();
          permitJoinMasterDevice(0);
          emberEventControlSetInactive(permitJointTimeoutEventControl);
          connect.state = CONNECT_STATE_PAIRED;
          updateDisplay(0);
        } else {
          permitJoinSet = true;
          emberSetSecurityKey(&securityKey);
          permitJoinMasterDevice(PJOIN_TIMEOUT);
          emberEventControlSetDelayMS(permitJointTimeoutEventControl, (PJOIN_TIMEOUT * 1000));
          clearConnectFlag();
          connect.state = CONNECT_STATE_JOIN;
          updateDisplay(0);
        }
      }
    }
    break;
    case CONNECT_STATE_FORM:
    {
      flag = getConnectFlag();
      if (flag == CONNECT_BUTTON1_LONG_PRESS) {
        emberResetNetworkState();
        clearConnectFlag();
        emberAfCorePrintln("[C] STATE PJOIN: MSG CONNECT_BUTTON1_PRESSED Reset");
        demo.connConnectInUse = false;
      } else if (flag == CONNECT_BUTTON1_PRESSED) {
        startEnergyScan();
        clearConnectFlag();
        connect.state = CONNECT_STATE_JOIN;
        emberAfCorePrintln("[C] STATE PJOIN: MSG CONNECT_BUTTON1_PRESSED Pjoin");
        updateDisplay(0);
      }
    }
    break;
    case CONNECT_STATE_INVALID:
      srand(halCommonGetInt16uMillisecondTick());
      connect.networkParams.panId = rand();
      connect.state = CONNECT_STATE_FORM;
      break;
    default:
      break;
  }
}

void emberAfIncomingMessageCallback(EmberIncomingMessage *message)
{
  emberAfCorePrintln("RX message received: ");
  if (message->length < CONNECT_LIGHT_MINIMUM_LENGTH
      || (emberFetchLowHighInt16u(message->payload
                                  + CONNECT_LIGHT_PROTOCOL_ID_OFFSET)
          != CONNECT_LIGHT_PROTOCOL_ID)) {
    return;
  }

  switch (message->payload[CONNECT_LIGHT_COMMAND_ID_OFFSET]) {
    case CONNECT_LIGHT_COMMAND_ID_DATA:
    {
      uint8_t i;
      emberAfCorePrint("RX: Data from 0x%2x:", message->source);
      for (i = CONNECT_LIGHT_DATA_OFFSET; i < message->length; i++) {
        emberAfCorePrint(" %x", message->payload[i]);
      }
      emberAfCorePrintln("");
      emebrAppReceiveEventHandler(message->source,
                                  &message->payload[CONNECT_LIGHT_DATA_OFFSET],
                                  message->length - CONNECT_LIGHT_DATA_OFFSET);
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
  switch (status) {
    case EMBER_NETWORK_UP:
      emberAfCorePrintln("Network up");
      saveConnectParam(CONNECT_STATE_JOIN, 0);
      setConnectFlag(CONNECT_JOINED);
      // If the node joined as a sleepy slave start short polling.
      emberAfPluginPollEnableShortPolling(emberGetNodeType()
                                          == EMBER_STAR_SLEEPY_END_DEVICE);
      break;
    case EMBER_NETWORK_DOWN:
      emberAfCorePrintln("Network down");
      resetConnectParam();
      dmpUiDisplayHelp(true);
      break;
    case EMBER_JOIN_FAILED:
      emberAfCorePrintln("Join failed");
      break;
    default:
      emberAfCorePrintln("Stack status: 0x%x", status);
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
  saveConnectParam(CONNECT_STATE_JOIN, nodeId);
  setConnectFlag(CONNECT_JOINED);
}

#if defined(EMBER_AF_PLUGIN_MICRIUM_RTOS) && defined(EMBER_AF_PLUGIN_MICRIUM_RTOS_APP_TASK1)

// Simple application task that prints something every second.

void emberAfPluginMicriumRtosAppTask1InitCallback(void)
{
  emberAfCorePrintln("app task init");
}

#include <kernel/include/os.h>
#define TICK_INTERVAL_MS 1000

void emberAfPluginMicriumRtosAppTask1MainLoopCallback(void *p_arg)
{
  RTOS_ERR err;
  OS_TICK yieldTimeTicks = (OSCfg_TickRate_Hz * TICK_INTERVAL_MS) / 1000;

  while (true) {
    emberAfCorePrintln("app task tick");

    OSTimeDly(yieldTimeTicks, OS_OPT_TIME_DLY, &err);
  }
}

#endif // EMBER_AF_PLUGIN_MICRIUM_RTOS && EMBER_AF_PLUGIN_MICRIUM_RTOS_APP_TASK1

//------------------------------------------------------------------------------
// Static functions

static EmberStatus send(EmberNodeId nodeId,
                        connectLightCommandId commandId,
                        uint8_t *buffer,
                        uint8_t bufferLength)
{
  uint8_t message[EMBER_MAX_UNSECURED_APPLICATION_PAYLOAD_LENGTH];
  EmberMessageLength messageLength = 0;

  if ((bufferLength + CONNECT_LIGHT_HEADER_LENGTH)
      > ((txOptions & EMBER_OPTIONS_SECURITY_ENABLED)
         ? EMBER_MAX_SECURED_APPLICATION_PAYLOAD_LENGTH
         : EMBER_MAX_UNSECURED_APPLICATION_PAYLOAD_LENGTH)) {
    return EMBER_MESSAGE_TOO_LONG;
  }
  emberStoreLowHighInt16u(message + messageLength, CONNECT_LIGHT_PROTOCOL_ID);
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
                CONNECT_LIGHT_COMMAND_ID_DATA,
                payload,
                payloadLength);

  emberAfCorePrint("TX: Data to 0x%2x:{", connect.destNodeId);
  emberAfCorePrintBuffer(payload, payloadLength, TRUE);
  emberAfCorePrintln("}: status=0x%x", status);
  return status;
}

void halButtonIsr(uint8_t button,
                  uint8_t state)
{
  static uint16_t buttonPressTime;
  uint16_t currentTime = 0;
  if (state == BUTTON_PRESSED) {
    if (button == BUTTON1) {
      buttonPressTime = halCommonGetInt16uMillisecondTick();
    }
  } else if (state == BUTTON_RELEASED) {
    if (button == BUTTON0) {
      setConnectFlag(CONNECT_BUTTON0_PRESSED);
    }
    if (button == BUTTON1) {
      currentTime = halCommonGetInt16uMillisecondTick();

      if ((currentTime - buttonPressTime) > BUTTON_LONG_PRESS_TIME_MSEC) {
        resetConnectParam();
        setConnectFlag(CONNECT_BUTTON1_LONG_PRESS);
      } else {
        EmberNetworkStatus status = emberNetworkState();
        if (status == EMBER_NO_NETWORK) {
          setConnectFlag(CONNECT_BUTTON1_PRESSED);
        } else {
          setConnectFlag(CONNECT_PERMIT_JOIN);
        }
      }
    }
  }
}

void lcdDirectionDisplayEventHandler(void)
{
  emberEventControlSetInactive(lcdDirectionDisplayEventControl);
  updateDisplay(0);
}

void permitJointTimeoutEventHandler(void)
{
  emberEventControlSetInactive(permitJointTimeoutEventControl);
  setConnectFlag(CONNECT_PERMIT_JOIN);
}

void readConnectLightState(uint8_t *payload)
{
  *payload = demo.light;
}

void writeConnectLightState(CommandType_t cmd,
                            uint8_t *payload,
                            uint8_t payloadLen)
{
  if (cmd == COMMAND_RELAY_SET_REQ) {
    demo.light = payload[0];
    readConnectLightState(payload);
    setConnectFlag(CONNECT_LIGHT_SET_STATUS_REQ);
  }
}

void setEUI64FromResponse(uint8_t * payload, uint8_t payloadLen)
{
  if (payloadLen == EUI64_SIZE) {
    MEMCOPY(connect.destEui64, payload, payloadLen);
  }
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

// When we start as a master, we form a network and enable permit join.
void startMasterCommand(void)
{
  EmberStatus status;
  EmberNetworkParameters params;

  // Initialize the security key to the default key prior to forming the
  // network.
  emberSetSecurityKey(&securityKey);

  params.panId = CONNECT_LIGHT_PAN_ID;
  params.radioChannel = emberUnsignedCommandArgument(0);
  params.radioTxPower = txPower;

  status = emberFormNetwork(&params);

  if (status != EMBER_SUCCESS) {
    emberAfCorePrintln("Form failed, 0x%x", status);
  }
}

void setTxOptionsCommand(void)
{
  txOptions = emberUnsignedCommandArgument(0);
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
                            ? CONNECT_LIGHT_SLAVE_ID
                            : CONNECT_LIGHT_MASTER_ID;

  status = send(destination,
                CONNECT_LIGHT_COMMAND_ID_DATA,
                contents,
                length);

  emberAfCorePrint("TX: Data to 0x%2x:{", destination);
  emberAfCorePrintBuffer(contents, length, TRUE);
  emberAfCorePrintln("}: status=0x%x", status);
}

void startEnergyScanCommand(void)
{
  EmberStatus status;
  uint8_t channelToScan = emberUnsignedCommandArgument(0);
  uint8_t samples = emberUnsignedCommandArgument(1);
  status = emberStartEnergyScan(channelToScan, samples);

  if (status != EMBER_SUCCESS) {
    emberAfCorePrintln("Start energy scanning failed, status=0x%x", status);
  } else {
    emberAfCorePrintln("Start energy scanning: channel %d, samples %d",
                       channelToScan, samples);
  }
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
}
