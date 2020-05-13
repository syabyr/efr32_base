/***************************************************************************//**
 * @brief Application level joining functionality.
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

#ifndef __SILABS_APP_JOIN_H__
#define __SILABS_APP_JOIN_H__

#define BUFFER_DATA_SIZE (96) // This must be less than RAIL fifo size

// Message retry time in miliseconds.
#define MESSSAGE_RETRY_TIME 300
// Retry Attempts
#define MESSAGE_RETRY_ATTEMPT 3

#define APP_COMMAND_FC_COMMISSIONING_COMMAND  0x02
#define APP_COMMAND_FC_MANUFACTURE_ID_PRESENT 0x04
#define APP_COMMAND_FC_DIR_SERVER_TO_CLIENT   0x08
#define APP_COMMAND_FC_RESPONSE_REQUIRED      0x10

#define APP_COMMAND_FC_DIR_CLIENT_TO_SERVER   0x00

typedef enum {
  API_SUCCESS,
  API_FAILED,
  API_PAIRING_IN_PROGRESS,
  API_ALREADY_PAIRED,
  API_NOISE_SCAN_IN_PROGRESS,
}ApiStatus_t;

typedef enum {
  EMBER_NOT_PAIRED,
  EMBER_PAIRING_STAGE_1,
  EMBER_PAIRING_STAGE_2,
  EMBER_PAIRING_STAGE_3,
  EMBER_PAIRED,
  EMBER_NOISE_SCAN,
  EMBER_FIND_TO_PAIR,
  EMBER_OPEN_TO_PAIR,
}PairingState_t;

typedef enum {
  COMMAND_PAIRING_INIT_REQ,
  COMMAND_PAIRING_INIT_RESP,
  COMMAND_PAIRING_GETPUBLICKEY_REQ,
  COMMAND_PAIRING_GETPUBLICKEY_RESP,
  COMMAND_PAIRING_FINAL_REQ,
  COMMAND_PAIRING_FINAL_RESP,
  COMMAND_RELAY_SET_REQ,
  COMMAND_RELAY_SET_RESP,
  COMMAND_RELAY_READ_REQ,
  COMMAND_RELAY_READ_RESP,
  COMMAND_READ_EUI64_REQ,
  COMMAND_READ_EUI64_RESP,
  COMANAD_NONE = 0xFF
}CommandType_t;

typedef struct {
  CommandType_t command;
  uint8_t length;
  uint8_t *payload;
  CommandType_t commandResp;
  uint16_t commandRespTimeout;
}Command_t;

typedef struct {
  CommandType_t sentReq;
  CommandType_t waitingResp;
  uint8_t retryAttempt;
  uint16_t retryInterval;
  uint16_t respTimeout;
  uint8_t reqSeqNo;
}ReqResp_t;

typedef struct {
  uint8_t data[BUFFER_DATA_SIZE];
  uint8_t length;
  //union {
  bool busy;
  bool dataAvailable;
  //}status;
}Buffer_t;

typedef enum {
  CONNECT_STATE_INVALID,
  CONNECT_STATE_FORM,
  CONNECT_STATE_JOIN,
  CONNECT_STATE_KEY_EXCHANGE,
  CONNECT_STATE_PAIRED
} connectState_e;

typedef enum {
  CONNECT_MSG_INVALID,
  CONNECT_BUTTON0_PRESSED = 1,
  CONNECT_BUTTON1_PRESSED,
  CONNECT_BUTTON1_LONG_PRESS,
  CONNECT_RETRY_ATTEMPT_FAIL,
  CONNECT_JOINED,
  CONNECT_JOIN_FAIL,
  CONNECT_PERMIT_JOIN,
  CONNECT_KEY_EXCHANGE,
  CONNECT_KEY_EXCHANGE_SUCCESS,
  CONNECT_KEY_EXCHANGE_FAIL,
  CONNECT_LIGHT_SET_STATUS_REQ,
  CONNECT_LIGHT_SET_STATUS_RESP,
  CONNECT_LIGHT_READ_STATUS_REQ,
  CONNECT_LIGHT_READ_STATUS_RESP,
  CONNECT_READ_EUI64_REQ,
  CONNECT_READ_EUI64_RESP,
}connectMsg;

typedef struct {
  connectState_e state;
  EmberNodeId sourceNodeId;
  EmberEUI64  sourceEui64;
  EmberNodeId destNodeId;
  EmberEUI64  destEui64;
  EmberNetworkParameters networkParams;
}connect_t;

typedef struct {
  PairingState_t pairingState;
  uint16_t pairingOpenTime;
  uint8_t pairingAttempts;
  uint8_t useSecurity;
  //PairSecurity_t security;
  connect_t * pConnect;
  ReqResp_t * reqResp;
  Buffer_t * txBuffer;
  Buffer_t * rxBuffer;
}Pair_t;

// Public Interfaces
connect_t * getConnectParam(void);
int emberAppMbedTlsInitContext(void);
int emberAppMbedTlsGenerateKeyPair(void);
int emberAppMbedTlsCalculateSharedSecret(void);
void emberAppMbedtlsKeyExchangeCallback(void);
void emebrAppTransmitEventHandler(uint8_t status);
void emebrAppReceiveEventHandler(uint16_t sourceId,
                                 uint8_t * rxPacket,
                                 uint8_t length);
ApiStatus_t emberSendPairInitRequest(EmberEUI64 sourceEui64,
                                     bool keyExchange);
ApiStatus_t emberSendMsgReq(uint32_t myId,
                            bool secure,
                            CommandType_t command,
                            uint8_t *payload,
                            uint8_t payloadLen);
ApiStatus_t emberSendMsgResp(uint32_t myId,
                             bool secure,
                             CommandType_t command,
                             uint8_t *payload,
                             uint8_t payloadLen);
EmberStatus connectDataSend(void *payload,
                            uint8_t payloadLength);
void readConnectLightState(uint8_t *payload);
void writeConnectLightState(CommandType_t cmd,
                            uint8_t *payload,
                            uint8_t payloadLen);
void setEUI64FromResponse(uint8_t * payload,
                          uint8_t payloadLen);
void emberAppMbedtlsKeyExchangeNegotiatedKey(uint8_t *key,
                                             uint8_t keylength);
#endif
