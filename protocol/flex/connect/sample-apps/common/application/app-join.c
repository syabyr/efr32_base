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
#include PLATFORM_HEADER
#include CONFIGURATION_HEADER
#include "stack/include/ember.h"
#include "hal/hal.h"
#include "debug-print/debug-print.h"
#include "application/app-join.h"

// Externs
extern unsigned char cli_srv_exchange_out[32];
extern unsigned char cli_srv_exchange_in[32];

// Node definition
extern connect_t connect;
static Pair_t thisNode = { .pConnect = &connect };
static Buffer_t txBuffer;
static Buffer_t rxBuffer;
static ReqResp_t reqResp;
static uint8_t reqSequenceNumber = 0;

// Timer event control
EmberEventControl sendRetryTmrEventControl;
EmberEventControl responseTimeoutTmrEventControl;

#if APP_SECURITY_ENABLE
/**************************************************************************//**
 * Validates the state tarnsition
 *
 * @param currentState is pointer to current state
 * @param nextState next state
 * @returns Returns true if teh transition is a valid one.
 *
 * This function validates the state, changes and return true.
 * This ensure there is no bad transitions happening outside the defined paths.
 * this function should be used for changing the state in this file.
 *
 *****************************************************************************/
static bool stateTransitions(PairingState_t *currentState,
                             PairingState_t nextState)
{
  bool ret = false;
  // define all the possible current state and next state combinitions
  if (*currentState == EMBER_NOT_PAIRED
      && nextState == EMBER_OPEN_TO_PAIR) {
    *currentState = nextState;
    emberAfAppPrintln("EMBER_NOT_PAIRED->EMBER_OPEN_TO_PAIR\r\n");
    ret = true;
  } else if (*currentState == EMBER_NOT_PAIRED
             && nextState == EMBER_FIND_TO_PAIR) {
    *currentState = nextState;
    emberAfAppPrintln("EMBER_NOT_PAIRED->EMBER_FIND_TO_PAIR\r\n");
    ret = true;
  } else if (*currentState == EMBER_NOT_PAIRED
             && nextState == EMBER_NOISE_SCAN) {
    *currentState = nextState;
    emberAfAppPrintln("EMBER_NOT_PAIRED->EMBER_NOISE_SCAN\r\n");
    ret = true;
  } else if (*currentState == EMBER_NOISE_SCAN
             && nextState == EMBER_NOT_PAIRED) {
    *currentState = nextState;
    emberAfAppPrintln("EMBER_NOISE_SCAN->EMBER_NOT_PAIRED\r\n");
    ret = true;
  } else if (*currentState == EMBER_OPEN_TO_PAIR
             && nextState == EMBER_PAIRING_STAGE_1) {
    *currentState = nextState;
    emberAfAppPrintln("EMBER_OPEN_TO_PAIR->EMBER_PAIRING_STAGE_1\r\n");
    ret = true;
  } else if (*currentState == EMBER_FIND_TO_PAIR
             && nextState == EMBER_PAIRING_STAGE_1) {
    *currentState = nextState;
    emberAfAppPrintln("EMBER_FIND_TO_PAIR->EMBER_PAIRING_STAGE_1\r\n");
    ret = true;
  } else if (*currentState == EMBER_PAIRING_STAGE_1
             && nextState == EMBER_PAIRING_STAGE_2) {
    *currentState = nextState;
    emberAfAppPrintln("EMBER_PAIRING_STAGE_1->EMBER_PAIRING_STAGE_2\r\n");
    ret = true;
  } else if (*currentState == EMBER_PAIRING_STAGE_2
             && nextState == EMBER_PAIRED) {
    *currentState = nextState;
    emberAfAppPrintln("EMBER_PAIRING_STAGE_2->EMBER_PAIRED\r\n");
    ret = true;
  } else if (*currentState == (EMBER_PAIRING_STAGE_1 || EMBER_PAIRING_STAGE_2 || EMBER_PAIRING_STAGE_3)
             && nextState == EMBER_NOT_PAIRED) {
    *currentState = nextState;
    emberAfAppPrintln("EMBER_PAIRING_STAGE_x->EMBER_NOT_PAIRED\r\n");
    ret = true;
  } else {
    // assert as it not a correct state transition
  }
  return ret;
}
#endif //APP_SECURITY_ENABLE

// Gets next sequence number for request messages
static uint8_t getNextReqSequence(void)
{
  return (reqSequenceNumber++);
}

// Gets sequence number for response messages, which is same as
// request seq number.
static uint8_t getNextRespSequence(void)
{
  return reqResp.reqSeqNo;
}

/**************************************************************************//**
 * Response time out event handler.
 *
 * @param void
 * @returns void
 *
 * When an expected response for an outgoing request is timed out, this function
 * is called to clear out the reqResp structure.
 *
 *****************************************************************************/
void responseTimeoutTmrEventHandler(void)
{
  emberEventControlSetInactive(responseTimeoutTmrEventControl);
  emberAfAppPrintln("Response Timeout");
  //response timeout , so clean up reqResp
  reqResp.respTimeout = 0;
  reqResp.retryAttempt = 0;
  reqResp.retryInterval = 0;
  reqResp.sentReq = COMANAD_NONE;
  reqResp.waitingResp = COMANAD_NONE;
}

/**************************************************************************//**
 * Send retry time out event handler.
 *
 * @param void
 * @returns void
 *
 * When a send message that has requested a number of retries while submitted
 * to be sent out, a send retry timer is started and responsible for retrying
 * the requested number of attempts to resend the message from outgoing reqResp
 * structure.
 * This handler is called by the retry event timeout for each individual retry
 * attempts.
 *
 *****************************************************************************/
void sendRetryTmrEventHandler(void)
{
  emberAfAppPrintln("TX Retry %d", reqResp.retryAttempt);
  emberEventControlSetInactive(sendRetryTmrEventControl);
  // Decrement the attempt count.
  if (reqResp.retryAttempt) {
    reqResp.retryAttempt--;
  } else {
    return;
  }
  // start a retry of the same buffer directly
  ApiStatus_t statius = (ApiStatus_t) connectDataSend(txBuffer.data, txBuffer.length);
  if (statius != API_SUCCESS
      && reqResp.retryAttempt) {
    // If the send is not successful and more retries are remaining, retry.
    emberEventControlSetDelayMS(sendRetryTmrEventControl, reqResp.retryInterval);
  }
}

/**************************************************************************//**
 * Application message send function.
 *
 * @param fc frame counter
 * @param manufactureId manufacture id.
 * @param seqNo out going sequence number.
 * @param command outgoing command.
 * @param security security to be used in outgoing command.
 * @returns Returns status of command submission to stack.
 *
 * This function is a application specefic send function for all outgoing
 * command - request and response both. Primary function is that it serialises
 * the submitted command. It does not really send the comand out immediately
 * rather it submits the outgoing command to the the stack.
 *
 *****************************************************************************/
static ApiStatus_t emberAppSend(uint8_t fc,
                                uint16_t manufactureId,
                                uint8_t seqNo,
                                Command_t *command,
                                bool security)
{
  //<--------------------Application Command Format--------------->|
  //---------------------------------------------------------------|
  // FC | ManufacId | Seq No | CmdId | Cmd Length | Cmd Payload    |
  //  1 |    0/2    |   1    |  1    |    0/1     |       var      |
  //---------------------------------------------------------------|
  txBuffer.busy = true;
  uint8_t * pTxBuff = &(txBuffer.data[0]);
  *pTxBuff = fc;
  pTxBuff++;
  if (fc & APP_COMMAND_FC_MANUFACTURE_ID_PRESENT) {
    emberStoreLowHighInt16u(pTxBuff, manufactureId);
    pTxBuff += sizeof(uint16_t);
  }
  *pTxBuff = seqNo;
  pTxBuff++;
  *pTxBuff = command->command;
  pTxBuff++;
  if (command->length) {
    // Command length
    *pTxBuff = command->length;
    pTxBuff++;
    // command payload
    MEMCOPY(pTxBuff, command->payload, command->length);
    pTxBuff += command->length;
  }
  uint16_t length = pTxBuff - &(txBuffer.data[0]);
  txBuffer.length = length;
  reqResp.retryAttempt = MESSAGE_RETRY_ATTEMPT;
  reqResp.retryInterval = MESSSAGE_RETRY_TIME;
  reqResp.sentReq = command->command;
  reqResp.waitingResp = command->commandResp;
  reqResp.respTimeout = command->commandRespTimeout;
  reqResp.reqSeqNo = seqNo;
  // call Connect transmission
  return connectDataSend(txBuffer.data, txBuffer.length);;
}

#if APP_SECURITY_ENABLE
/**************************************************************************//**
 * Application paring request.
 *
 * @param sourceEui64 Request node IEEE address.
 * @param keyExchange If application specefic key exchange needed.
 * @returns status of the request submitted to outgoing send function.
 *
 * When a node joins the network, this function is used to start a
 * application pairing request by the joining device with the coordinator.
 * This request is associated with expectation of a response.
 *****************************************************************************/
ApiStatus_t emberSendPairInitRequest(EmberEUI64 sourceEui64, bool keyExchange)
{
  uint8_t payload[9] = { 0 };
  MEMCOPY(payload, sourceEui64, EUI64_SIZE);
  payload[8] = (uint8_t)keyExchange;
  // build the join payload
  Command_t pairInitReq = {
    .command = COMMAND_PAIRING_INIT_REQ,
    .commandResp = COMMAND_PAIRING_INIT_RESP,
    .length = (EUI64_SIZE + 1),
    .payload = payload,
    .commandRespTimeout = 10000,// 10 sec timeout
  };
  uint8_t fc = APP_COMMAND_FC_DIR_CLIENT_TO_SERVER
               | APP_COMMAND_FC_RESPONSE_REQUIRED
               | APP_COMMAND_FC_COMMISSIONING_COMMAND;
  uint8_t seqNo = getNextReqSequence();
  return emberAppSend(fc, 0, seqNo, &pairInitReq, false);
}
/**************************************************************************//**
 * Application paring final request.
 *
 * @param keyExchange If application specefic key exchange needed.
 * @returns status of the request submitted to outgoing send function.
 *
 * This request is to complete the final states of allication level pairing
 * process that started with emberSendPairInitRequest. If a key establishment
 * was request at the in the above request then this request will indicate the
 * final acceptance of the key exchange.
 *****************************************************************************/
static ApiStatus_t emberSendPairFinalRequest(bool keyExchange)
{
  // TODO : based on the key exchanged already - the message to be encrypted
  // using the established key
  uint8_t final_msg[14] = { 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01 };
  // build the pair final request payload
  Command_t pairFinalReq = {
    .command = COMMAND_PAIRING_FINAL_REQ,
    .commandResp = COMMAND_PAIRING_FINAL_RESP,
    .length = sizeof(final_msg),
    .payload = final_msg,
    .commandRespTimeout = 10000,// 10 sec timeout
  };
  uint8_t fc = APP_COMMAND_FC_DIR_CLIENT_TO_SERVER
               | APP_COMMAND_FC_RESPONSE_REQUIRED
               | APP_COMMAND_FC_COMMISSIONING_COMMAND;
  uint8_t seqNo = getNextReqSequence();
  return emberAppSend(fc, 0, seqNo, &pairFinalReq, true);
}
/**************************************************************************//**
 * Application level key exchange public key request.
 *
 * @param void
 * @returns status of the request submitted to outgoing send function.
 *
 * This function is a part of the application specefic key exchange framework.
 * It typically called when the emberSendPairInitRequest has received a response
 * indicating the responder is ready to do a key exchange. This function triggers
 * mbedTLS ECDH key exchange process with a set of agreed param, calculates its
 * public key then sends its own public key and expects a resposne from the remote
 * node containing remote node public key.
 *****************************************************************************/
ApiStatus_t emberSendPairGetPublicKeyRequest(void)
{
  // This is where it starts the mbedtls
  if (0 == emberAppMbedTlsInitContext()) {
    // Initialization and seeding random number is success
    // generate key pair
    if (0 == emberAppMbedTlsGenerateKeyPair()) {
      // key pair gets generated and the array that needs to go out is cli_srv_exchange_out
    } else {
      // TODO : handle retry or abort
    }
  } else {
    // TODO : handle retry or abort
  }

  // build Public Key Request payload
  Command_t getPublicKeyReq = {
    .command = COMMAND_PAIRING_GETPUBLICKEY_REQ,
    .commandResp = COMMAND_PAIRING_GETPUBLICKEY_RESP,
    .length = sizeof(cli_srv_exchange_out),
    .payload = cli_srv_exchange_out,
    .commandRespTimeout = 10000,// 1 sec timeout
  };
  uint8_t fc = APP_COMMAND_FC_DIR_CLIENT_TO_SERVER
               | APP_COMMAND_FC_RESPONSE_REQUIRED
               | APP_COMMAND_FC_COMMISSIONING_COMMAND;
  uint8_t seqNo = getNextReqSequence();
  return emberAppSend(fc, 0, seqNo, &getPublicKeyReq, false);
}
#endif //APP_SECURITY_ENABLE
/**************************************************************************//**
 * Generic request send function wrapper.
 *
 * @param myId short source address.
 * @param secure security usage in send request.
 * @param command request command.
 * @param payload request command payload.
 * @param payloadLen request command payload length
 * @returns status of the request submitted to outgoing send function.
 *
 * This function is a part of generic request send function to the pair node.
 *****************************************************************************/
ApiStatus_t emberSendMsgReq(uint32_t myId,
                            bool secure,
                            CommandType_t command,
                            uint8_t *payload,
                            uint8_t payloadLen)
{
  Command_t sendMsg = {
    .command = command,
    .commandResp = (command + 1),
    .length = payloadLen,
    .payload = payload,
    .commandRespTimeout = 5000,// 5 sec timeout
  };
  uint8_t fc = APP_COMMAND_FC_DIR_CLIENT_TO_SERVER
               | APP_COMMAND_FC_RESPONSE_REQUIRED;
  uint8_t seqNo = getNextReqSequence();
  return emberAppSend(fc, 0, seqNo, &sendMsg, false);
}

#if APP_SECURITY_ENABLE
/**************************************************************************//**
 * Pairing Init Response.
 *
 * @param seqNo sequence number for the response.
 * @param sourceEui64 responder IEEE address.
 * @param keyExchange if the responder supports key exchange.
 * @returns status of the request submitted to outgoing send function.
 *
 * This function is a response to the pairing init request and part of the
 * application specefic pairing mechanism.
 *****************************************************************************/
static ApiStatus_t emberSendPairInitResp(uint8_t seqNo,
                                         EmberEUI64 sourceEui64,
                                         bool keyExchange)
{
  uint8_t payload[9] = { 0 };
  MEMCOPY(payload, sourceEui64, EUI64_SIZE);
  payload[8] = (uint8_t)keyExchange;
  Command_t pairInitResp = {
    .command = COMMAND_PAIRING_INIT_RESP,
    .commandResp = COMANAD_NONE,
    .length = (EUI64_SIZE + 1),
    .payload = payload,
  };
  uint8_t fc = APP_COMMAND_FC_DIR_SERVER_TO_CLIENT
               | APP_COMMAND_FC_COMMISSIONING_COMMAND;
  return emberAppSend(fc, 0, seqNo, &pairInitResp, false);
}
/**************************************************************************//**
 * Pairing Final Response.
 *
 * @param seqNo sequence number for the response.
 * @returns status of the request submitted to outgoing send function.
 *
 * This function is a response to the pairing final request and part of the
 * application specefic pairing finalisation mechanism. This function typically
 * a predefined final message.
 * @note : In case of a key exchange this final message can be entryped as a
 * challenge.
 *****************************************************************************/
static ApiStatus_t emberSendPairFinalResp(uint8_t seqNo)
{
  uint8_t final_msg[14] = { 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02 };

  Command_t pairFinalResp = {
    .command = COMMAND_PAIRING_FINAL_RESP,
    .commandResp = COMANAD_NONE,
    .length = sizeof(final_msg),
    .payload = final_msg,
  };
  uint8_t fc = APP_COMMAND_FC_DIR_SERVER_TO_CLIENT
               | APP_COMMAND_FC_COMMISSIONING_COMMAND;
  return emberAppSend(fc, 0, seqNo, &pairFinalResp, true);
}
/**************************************************************************//**
 * Application level response to publick key request.
 *
 * @param seqNo sequence number for the response
 * @returns status of the request submitted to outgoing send function.
 *
 * This function is a part of the application specefic key exchange framework.
 * It is called to respond to the publick key request. This function is triggered
 * mbedTLS ECDH key exchange process with a set of agreed param, calculates its
 * after the responder has finished calculating the public using a pre-defined
 * mbedTLS ECDH parameters (i.e same set as used by the remote node).
 *****************************************************************************/
static ApiStatus_t emberSendPairGetPublicKeyResp(uint8_t seqNo)
{
  Command_t getPublicKeyResp = {
    .command = COMMAND_PAIRING_GETPUBLICKEY_RESP,
    .commandResp = COMANAD_NONE,
    .length = sizeof(cli_srv_exchange_out), //sizeof(payload_pairing_publickey),
    .payload = cli_srv_exchange_out,
    .commandRespTimeout = 10000,// 1 sec timeout
  };
  uint8_t fc = APP_COMMAND_FC_DIR_SERVER_TO_CLIENT
               | APP_COMMAND_FC_COMMISSIONING_COMMAND;
  return emberAppSend(fc, 0, seqNo, &getPublicKeyResp, false);
}

#endif //APP_SECURITY_ENABLE
/**************************************************************************//**
 * Generic response send function wrapper.
 *
 * @param myId short source address.
 * @param secure security usage in send request.
 * @param command request command.
 * @param payload request command payload.
 * @param payloadLen request command payload length
 * @returns status of the request submitted to outgoing send function.
 *
 * This function is a part of generic response send function to the paired node.
 *****************************************************************************/
ApiStatus_t emberSendMsgResp(uint32_t myId,
                             bool secure,
                             CommandType_t command,
                             uint8_t *payload,
                             uint8_t payloadLen)
{
  Command_t sendMsgResp = {
    .command = command,
    .commandResp = COMANAD_NONE,
    .length = payloadLen,
    .payload = payload,
    .commandRespTimeout = 10000,
  };
  uint8_t fc = APP_COMMAND_FC_DIR_SERVER_TO_CLIENT;
  uint8_t seqNo = getNextRespSequence();
  return emberAppSend(fc, 0, seqNo, &sendMsgResp, false);
}

#if APP_SECURITY_ENABLE

/**************************************************************************//**
 * Processing of Pairing Request.
 *
 * @param seqNo Sequence number.
 * @param partnerId remote node short address.
 * @param payload request command payload.
 * @param payloadLen request command payload length
 * @returns void.
 *
 * This function is called to process/parse the Pairing request and generate/send a
 * response back to the remote node.
 *****************************************************************************/
static void processPairingInitReqCommand(uint8_t seqNo,
                                         uint16_t partnerId,
                                         uint8_t *payload,
                                         uint8_t payloadLength)
{
  emberAfAppPrintln("Processing : Pairing Init Req");
  if (payloadLength != 9) {
    emberAfAppPrintln("....DROP : Bad length");
    return;
  }
  // set partnerId
  thisNode.pConnect->destNodeId = partnerId;
  // Pairing Init Request command payload
  //| 8 bytes EUI64 | 1 Byte Security|
  MEMCOPY(thisNode.pConnect->destEui64, payload, EUI64_SIZE);
  bool security = (bool)payload[8];

  stateTransitions(&thisNode.pairingState, EMBER_PAIRING_STAGE_1);
  // TODO : read thisNode to find out if the node supports a
  // key exchange and pass through the response `security`instead.
  emberSendPairInitResp(seqNo, emberGetEui64(), security);
}
/**************************************************************************//**
 * Processing of Pairing Response.
 *
 * @param seqNo Sequence number.
 * @param partnerId remote node short address.
 * @param payload request command payload.
 * @param payloadLen request command payload length
 * @returns void.
 *
 * This function is called to process/parse the Pairing response. Based on the
 * security in the incomming payload it triggers the key exchange mechanism, that
 * sends out a public key request to remote node.
 *****************************************************************************/
static void processPairingInitRespCommand(uint8_t seqNo,
                                          uint16_t partnerId,
                                          uint8_t *payload,
                                          uint8_t payloadLength)
{
  emberAfAppPrintln("Processing : Pairing Init Resp");
  if (payloadLength != 9) {
    emberAfAppPrintln("....DROP : Bad length");
    return;
  }
  if (reqResp.waitingResp != COMMAND_PAIRING_INIT_RESP) {
    emberAfAppPrintln(".....DROP : Unsolicitaed response!");
    return;
  }
  thisNode.pConnect->destNodeId = partnerId;
  // Pairing Init Request command payload
  //| 8 bytes EUI64 | 1 Byte Security|
  MEMCOPY(thisNode.pConnect->destEui64, payload, EUI64_SIZE);
  bool security = (bool)payload[8];

  stateTransitions(&thisNode.pairingState, EMBER_PAIRING_STAGE_1);
  // TODO : This block should actually decide weather to go with this device
  // or not because if the device is not supporting key exchange is it worth
  // to go ahead with an unsecured application communication.
  if (security == true) {
    emberSendPairGetPublicKeyRequest();
  } else {
    // send pairing final request
    emberSendPairFinalRequest(false);
  }
}
/**************************************************************************//**
 * Processing of Public Key Request.
 *
 * @param seqNo Sequence number.
 * @param payload request command payload.
 * @param payloadLen request command payload length
 * @returns void.
 *
 * This function is a part of the application specefic key exchange framework.
 * Upon reception of the request for public key, this function triggers the
 * mbedTLS ECDH validate sthe received public key and calculates the sahred secret.
 * And finally sends its own public key back to the requester.
 *****************************************************************************/
static void processPairingGetPublicKeyReqCommand(uint8_t seqNo,
                                                 uint8_t * payload,
                                                 uint8_t payload_len)
{
  emberAfAppPrintln("Processing : Pairing Get Public Key Req");
  if (payload_len != sizeof (cli_srv_exchange_in)) {
    emberAfAppPrintln(".... DROP : Bad length");
    // TODO : Inform the other side with a failure message,
    //        this indicates the final request without any security
    return;
  }
  // Start of mbedtls stuff for server here
  // This is where it starts the mbedtls
  if (0 == emberAppMbedTlsInitContext()) {
    // Initialization and seeding random number is success
    // generate key pair
    if (0 == emberAppMbedTlsGenerateKeyPair()) {
      // key pair gets generated and the array that needs to go out is cli_srv_exchange_out
    } else {
      // TODO : Retry or abort
    }
  } else {
    // TODO : Retry or abort
  }

  // Receive and copy the key that client has sent
  MEMCOPY(cli_srv_exchange_in, payload, 32);

  // Calculate the shared secrete
  emberAppMbedTlsCalculateSharedSecret();
  // TODO : send pairing response if the mbedTLS is all OK else
  //        send abort message - may be a new message to be derived
  emberSendPairGetPublicKeyResp(seqNo);
}
/**************************************************************************//**
 * Processing of Public Key Response.
 *
 * @param seqNo Sequence number.
 * @param payload request command payload.
 * @param payloadLen request command payload length
 * @returns void.
 *
 * This function is a part of the application specefic key exchange framework.
 * Upon reception of the Public key response from remote node, this function
 * triggers the mbedTLS ECDH to validate the remote public key and continue to
 * calculates the sahred secret and sends the pairing finalisation request.
 *****************************************************************************/
static void processPairingGetPublicKeyRespCommand(uint8_t seqNo,
                                                  uint8_t * payload,
                                                  uint8_t payload_len)
{
  emberAfAppPrintln("Processing : Pairing Get Public Key Resp");
  if (payload_len != sizeof (cli_srv_exchange_in)) {
    emberAfAppPrintln(".... DROP : Bad length");
    // TODO : Inform the other side with a failure message,
    //        this indicates the final request without any security
    emberSendPairFinalRequest(false);
    return;
  }
  // Receive and copy the key that client has sent, to be used to calculate
  // shared secrete
  MEMCOPY(cli_srv_exchange_in, payload, payload_len);

  // Calculate the shared secret
  emberAppMbedTlsCalculateSharedSecret();
  // TODO : This can indicate the status of the key exchange
  //    -- hard coded to true for sample app.
  emberSendPairFinalRequest(true);
}
/**************************************************************************//**
 * Processing of Pairing Final Request.
 *
 * @param seqNo Sequence number.
 * @returns void.
 *
 * Processes the pairing finalisation response and informs the user the conclusion
 * of key exchnage through a callback.
 *****************************************************************************/
static void processPairingFinalReqCommand(uint8_t seqNo)
{
  emberAfAppPrintln("Processing : Pairing Final Req");
  stateTransitions(&thisNode.pairingState, EMBER_PAIRED);
  // send pairing response
  emberSendPairFinalResp(seqNo);
  emberAppMbedtlsKeyExchangeCallback();
}
/**************************************************************************//**
 * Processing of Pairing Final Response.
 *
 * @param seqNo Sequence number.
 * @returns void.
 *
 * Processes the pairing finalisation response and informs the user the conclusion
 * of key exchnage through a callback.
 *****************************************************************************/
static void processPairingFinalRespCommand(uint8_t seqNo)
{
  emberAfAppPrintln("Processing : Pairing Final Resp");
  if (reqResp.waitingResp != COMMAND_PAIRING_FINAL_RESP) {
    emberAfAppPrintln(".... DROP : Unsoliciated Response");
    return;
  }
  stateTransitions(&thisNode.pairingState, EMBER_PAIRED);
  emberAppMbedtlsKeyExchangeCallback();
}
#endif //APP_SECURITY_ENABLE

/**************************************************************************//**
 * General processing of remote EUI64 read response command.
 *
 * @param payload EUI64.
 * @param payloadLen EUI64.
 * @returns void.
 *
 * Receives the EUI64 of remote node and lets application save it.
 *****************************************************************************/
static void processReadEUI64RespCommand(uint8_t *payload, uint8_t payloadLen)
{
  // The relay state read response , solicited means this should only be handled
  // when the read request was out earlier
  if (reqResp.waitingResp == COMMAND_READ_EUI64_RESP) {
    setEUI64FromResponse(payload, payloadLen);
  }
}
/**************************************************************************//**
 * General processing of remote EUI64 read command.
 *
 * @param void
 * @returns void.
 *
 * Sends responses with EUI64.
 *****************************************************************************/
static void processReadEUI64Command(void)
{
  uint8_t payload[EUI64_SIZE] = { 0 };
  MEMCOPY(payload, emberGetEui64(), EUI64_SIZE);
  emberSendMsgResp(thisNode.pConnect->sourceNodeId, false, COMMAND_READ_EUI64_RESP, payload, EUI64_SIZE);
}

/**************************************************************************//**
 * General processing of relay state read response.
 *
 * @param payload that holds relay state
 * @param payloadLen
 * @returns void.
 *
 * Application specefic relay command response processing.
 *****************************************************************************/
static void processReadRelayStateRespCommand(uint8_t *payload, uint8_t payloadLen)
{
  // The relay state read response , solicited means this should only be handled
  // when the read request was out earlier
  if (reqResp.waitingResp == COMMAND_RELAY_READ_RESP) {
    writeConnectLightState(COMMAND_RELAY_READ_RESP, payload, payloadLen);
  }
}
/**************************************************************************//**
 * General processing of read relay state request command.
 *
 * @param payload that holds relay state
 * @param payloadLen
 * @returns void.
 *
 * Sends a relay state response
 *****************************************************************************/
static void processReadRelayStateCommand(void)
{
  uint8_t payload = 0;
  readConnectLightState(&payload);
  emberSendMsgResp(thisNode.pConnect->sourceNodeId, false, COMMAND_RELAY_READ_RESP, &payload, 1);
}
/**************************************************************************//**
 * General processing of relay state response command.
 *
 * @param payload that holds relay state
 * @param payloadLen
 * @returns void.
 *
 * Sets the applicatiion state based on the relay state response
 *****************************************************************************/
static void processSetRelayStateRespCommand(uint8_t *payload, uint8_t payloadLen)
{
  // The relay state set response, this gets in when the switch set the status or BT sets the status
  // these two are solicited means just the confirmation to the set req.
  // But when light write it , it is unsolisited and should propagate to BLE the read request was out earlier
  if (reqResp.waitingResp != COMMAND_RELAY_SET_RESP) {
    writeConnectLightState(COMMAND_RELAY_SET_RESP, payload, payloadLen);
  }
}
/**************************************************************************//**
 * General processing of set relay state request command.
 *
 * @param payload that holds relay state
 * @param payloadLen
 * @returns void.
 *
 * Sends a relay state response
 *****************************************************************************/
static void processSetRelayStateCommand(uint8_t *payload, uint8_t payloadLen)
{
  writeConnectLightState(COMMAND_RELAY_SET_REQ, payload, payloadLen);
  readConnectLightState(payload);
  emberSendMsgResp(thisNode.pConnect->sourceNodeId, false, COMMAND_RELAY_SET_RESP, payload, 1);
}
/**************************************************************************//**
 * General processing all the commands.
 *
 * @param sourceId of incoming message
 * @param cmd_data incomming command string
 * @param cmd_data_len data length
 * @returns void.
 *
 * This is a general parsing function based on the incomming command Ids.
 * Called fromt he receive handler.
 *****************************************************************************/
static void processCommand(uint16_t sourceId,
                           uint8_t *cmd_data,
                           uint8_t cmd_data_len)
{
  uint8_t fc;
  uint16_t manufactureId = 0;
  uint8_t seqNo;
  uint8_t command;
  uint8_t payload_length = 0;
  uint8_t * payload = NULL;
  uint8_t header = 0;
  //<--------------------Application Command Format--------------->|
  //---------------------------------------------------------------|
  // FC | ManufacId | Seq No | CmdId | Cmd Length | Cmd Payload    |
  //  1 |    0/2    |   1    |  1    |    0/1     |       var      |
  //---------------------------------------------------------------|
  fc = cmd_data[header];
  header++;
  if (fc & APP_COMMAND_FC_MANUFACTURE_ID_PRESENT) {
    manufactureId = cmd_data[header] + ((uint16_t)(cmd_data[header + 1]) << 8);
    header += 2;
  }
  seqNo = cmd_data[header];
  header++;
  command = cmd_data[header];
  header++;
  if (cmd_data_len > header) {
    // Pay load may be present now
    payload_length = cmd_data[header];
    header++;
    // Pay load length must match the integrity of the network packet received
    if (cmd_data_len != (header + payload_length)) {
      emberAfAppPrintln("Process Command : DROP : Bad frame");
      return;
    }
    if (payload_length) {
      payload = &(cmd_data[header]);
    }
  }

#if APP_SECURITY_ENABLE
  // Direction check
  if (((command == COMMAND_PAIRING_INIT_REQ
        || command == COMMAND_PAIRING_GETPUBLICKEY_REQ
        || command == COMMAND_PAIRING_FINAL_REQ)
       && (fc & APP_COMMAND_FC_DIR_SERVER_TO_CLIENT))
      || ((command == COMMAND_PAIRING_INIT_RESP
           || command == COMMAND_PAIRING_GETPUBLICKEY_RESP
           || command == COMMAND_PAIRING_FINAL_RESP)
          && !(fc & APP_COMMAND_FC_DIR_SERVER_TO_CLIENT))) {
    emberAfAppPrintln("Process Command : DROP : Bad direction");
    return;
  }
#endif //APP_SECURITY_ENABLE

  switch (command) {
#if APP_SECURITY_ENABLE
    case COMMAND_PAIRING_INIT_REQ:
    {
      processPairingInitReqCommand(seqNo, sourceId, payload, payload_length);
      break;
    }
    case COMMAND_PAIRING_INIT_RESP:
    {
      processPairingInitRespCommand(seqNo, sourceId, payload, payload_length);
      break;
    }
    case COMMAND_PAIRING_GETPUBLICKEY_REQ:
    {
      processPairingGetPublicKeyReqCommand(seqNo, payload, payload_length);
      break;
    }
    case COMMAND_PAIRING_GETPUBLICKEY_RESP:
    {
      processPairingGetPublicKeyRespCommand(seqNo, payload, payload_length);
      break;
    }
    case COMMAND_PAIRING_FINAL_REQ:
    {
      processPairingFinalReqCommand(seqNo);
      break;
    }
    case COMMAND_PAIRING_FINAL_RESP:
    {
      processPairingFinalRespCommand(seqNo);
      break;
    }
#endif //APP_SECURITY_ENABLE
    case COMMAND_RELAY_SET_REQ:
    {
      processSetRelayStateCommand(payload, payload_length);
    }
    break;
    case COMMAND_RELAY_SET_RESP:
    {
      processSetRelayStateRespCommand(payload, payload_length);
    }
    break;
    case COMMAND_RELAY_READ_REQ:
    {
      processReadRelayStateCommand();
    }
    break;
    case COMMAND_RELAY_READ_RESP:
    {
      processReadRelayStateRespCommand(payload, payload_length);
    }
    break;
    case COMMAND_READ_EUI64_REQ:
    {
      processReadEUI64Command();
    }
    break;
    case COMMAND_READ_EUI64_RESP:
    {
      processReadEUI64RespCommand(payload, payload_length);
    }
    break;
    default:
      break;
  }
  if (reqResp.waitingResp == command) {
    // cancel the response timeout timer
    responseTimeoutTmrEventHandler();
  }
}
/**************************************************************************//**
 * Application receive event handler.
 *
 * @param sourceId of incoming message
 * @param rxPacket incomming command string
 * @param length data length
 * @returns void.
 *
 * This a receive handler called from Connect receive handler. It makes a copy of
 * incoming data to allow freeing up of the buffers.
 *****************************************************************************/
void emebrAppReceiveEventHandler(uint16_t sourceId, uint8_t * rxPacket, uint8_t length)
{
  MEMCOPY(rxBuffer.data, rxPacket, length);
  rxBuffer.length = length;
  rxBuffer.dataAvailable = true;
  processCommand(sourceId, &rxBuffer.data[0], rxBuffer.length);
}
/**************************************************************************//**
 * Transmit event handler.
 *
 * @param status of incoming message
 * @returns void.
 *
 * Transmit event handler to ensure the outgoing request is retries incase it fails
 * transmission and upon success the reqResp structure is freed.
 *****************************************************************************/
void emebrAppTransmitEventHandler(uint8_t status)
{
  // The emberAfMessageSentCallback informed status
  if (status == 0) {
    // success, ensure to free the tx buffer;
    txBuffer.busy = false;
    // If the send is successful, the req is expecting a response,
    // then start response timeout.
    if (reqResp.waitingResp != COMANAD_NONE) {
      emberEventControlSetDelayMS(responseTimeoutTmrEventControl, reqResp.respTimeout);
    }
  } else if (status == 0x40 // send out but no MAC ACK or could not send CCA failure
             || status == 0x8D) {
    sendRetryTmrEventHandler();
  }
}
