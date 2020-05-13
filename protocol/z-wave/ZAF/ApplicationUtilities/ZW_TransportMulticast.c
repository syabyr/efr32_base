/**
 * @file ZW_TransportMulticast.c
 * @brief Handles multicast frames in the Z-Wave Framework.
 * @copyright 2019 Silicon Laboratories Inc.
 */

/****************************************************************************/
/*                              INCLUDE FILES                               */
/****************************************************************************/

#include <ZW_TransportMulticast.h>
#include <stdint.h>
#include <string.h> // For memset and memcpy
#include <NodeMask.h>
#include <misc.h>
#include <ZAF_tx_mutex.h>
#include <ZW_transport_api.h>
#include <ZW_TransportSecProtocol.h>
#include <association_plus.h>

//#define DEBUGPRINT
#include "DebugPrint.h"

/****************************************************************************/
/*                      PRIVATE TYPES and DEFINITIONS                       */
/****************************************************************************/

typedef enum
{
  MCTXRESULT_SUCCESS,
  MCTXRESULT_FAILURE,
  MCTXRESULT_NO_DESTINATIONS
}
MultiChannelTXResult_t;

typedef enum
{
  MULTICAST_TXRESULT_SUCCESS,
  MULTICAST_TXRESULT_FAILURE,
  MULTICAST_TXRESULT_NOT_ENOUGH_DESTINATIONS
}
MulticastTXResult_t;

/****************************************************************************/
/*                              PRIVATE DATA                                */
/****************************************************************************/

ZAF_TX_Callback_t p_callback_hold;

static uint8_t * p_data_hold;
static uint8_t data_length_hold;

static TRANSMIT_OPTIONS_TYPE_EX * p_nodelist_hold;

static bool gotSupervision = false;

static uint8_t singlecast_node_count;

static uint8_t fSupervisionEnableHold;

static uint8_t secKeys;

static uint8_t txSecOptions = 0;

static SApplicationHandles * m_pAppHandle;

static bool moreMultiChannelNodes;

static uint32_t remainingNodeCount;

/****************************************************************************/
/*                              EXPORTED DATA                               */
/****************************************************************************/

// Nothing here.

/****************************************************************************/
/*                            PRIVATE FUNCTIONS                             */
/****************************************************************************/

static void ZCB_callback_wrapper(uint8_t Status, TX_STATUS_TYPE* pStatusType);
static void ZCB_multicast_callback(TRANSMISSION_RESULT * pTransmissionResult);

static MultiChannelTXResult_t TransmitMultiChannel(transmission_result_t * pTxResult);
static MulticastTXResult_t TransmitMultiCast(transmission_result_t * pTxResult);

void ZW_TransportMulticast_Init(SApplicationHandles * pAppHandle)
{
  m_pAppHandle = pAppHandle;
}

void multichannel_callback(transmission_result_t * pTxResult)
{
  if (TRANSMIT_COMPLETE_OK != pTxResult->status) {
    /*
     * We have to check for TRANSMIT_COMPLETE_OK only because a multi channel destination node is
     * not allowed to respond with a Supervision Report. Hence, the transmission can never be
     * verified and so TRANSMIT_COMPLETE_VERIFIED is not relevant.
     *
     * In case the transmission failed, we stop and report it to the application.
     */
    pTxResult->isFinished = TRANSMISSION_RESULT_FINISHED;
    if (NULL != p_callback_hold) {
      p_callback_hold(pTxResult);
    }
    return;
  }

  if (true == moreMultiChannelNodes) {
    // There are more multi channel destinations
    pTxResult->isFinished = TRANSMISSION_RESULT_NOT_FINISHED;
    if (NULL != p_callback_hold) {
      p_callback_hold(pTxResult);
    }
    TransmitMultiChannel(NULL);
    return;
  }

  remainingNodeCount = AssociationGetSinglecastNodeCount();
  if (0 == remainingNodeCount) {
    // There are no other destinations than the multi channel bit addressing destinations.
    pTxResult->isFinished = TRANSMISSION_RESULT_FINISHED;
    if (NULL != p_callback_hold) {
      p_callback_hold(pTxResult);
    }
  } else {
    // There are remaining destinations.
    MulticastTXResult_t MulticastResult = TransmitMultiCast(NULL);
    if (MULTICAST_TXRESULT_FAILURE == MulticastResult) {
      pTxResult->status = TRANSMIT_COMPLETE_FAIL;
      pTxResult->isFinished = TRANSMISSION_RESULT_FINISHED;
      if (NULL != p_callback_hold) {
        p_callback_hold(pTxResult);
      }
      return;
    } else if (MULTICAST_TXRESULT_NOT_ENOUGH_DESTINATIONS == MulticastResult) {
      /*
      * If there's only one non-endpoint node, we skip the multicast and call
      * the multicast callback directly. This callback will initiate singlecast
      * transmission. The argument can be ignored.
      */
      ZCB_multicast_callback(NULL);
    } else {
      if (NULL != p_callback_hold) p_callback_hold(pTxResult);
    }
  }
}

/**
 * Checks whether there are any multi channel destinations and transmits the frame if so.
 * @return Returns true if the frame was enqueued. Otherwise false.
 */
static MultiChannelTXResult_t TransmitMultiChannel(transmission_result_t * pTxResult)
{
  destination_info_t node;

  do {
    memset((uint8_t *)&node, 0, sizeof(node));
    moreMultiChannelNodes = AssociationGetBitAdressingDestination(&(p_nodelist_hold->pList),
                                                                  &(p_nodelist_hold->list_length),
                                                                  &node);

    if (0 != node.node.BitAddress) {
      TRANSMIT_OPTIONS_TYPE_SINGLE_EX txOptions = {
                                                   .txOptions = p_nodelist_hold->txOptions,
                                                   .txSecOptions = 0,
                                                   .sourceEndpoint = p_nodelist_hold->sourceEndpoint,
                                                   .pDestNode = &node
      };
      EQueueNotifyingStatus txResult;
      /*
       * TODO: Ideally this transmission should not use verified delivery since the recipient is
       * not allowed to respond with Supervision Report.
       */
      txResult = Transport_SendRequestEP(
                    p_data_hold,
                    data_length_hold,
                    &txOptions,
                    multichannel_callback);

      return (EQUEUENOTIFYING_STATUS_SUCCESS == txResult) ? MCTXRESULT_SUCCESS : MCTXRESULT_FAILURE;
    }
  } while (true == moreMultiChannelNodes);

  return MCTXRESULT_NO_DESTINATIONS;
}

static MulticastTXResult_t TransmitMultiCast(transmission_result_t * pTxResult)
{
  NODE_MASK_TYPE node_mask;
  ZW_NodeMaskClear(node_mask, sizeof(NODE_MASK_TYPE));

  singlecast_node_count = 0;
  txSecOptions = 0;
  
  // Create transmit frame package
  SZwaveTransmitPackage FramePackage;
  memset(&FramePackage.uTransmitParams.SendDataMultiEx.FrameConfig.aFrame, 0, sizeof(FramePackage.uTransmitParams.SendDataMultiEx.FrameConfig.aFrame));
  memcpy(&FramePackage.uTransmitParams.SendDataMultiEx.FrameConfig.aFrame, p_data_hold, data_length_hold);
  FramePackage.uTransmitParams.SendDataMultiEx.FrameConfig.Handle = ZCB_callback_wrapper;
  FramePackage.uTransmitParams.SendDataMultiEx.FrameConfig.iFrameLength = data_length_hold;
  FramePackage.uTransmitParams.SendDataMultiEx.FrameConfig.TransmitOptions = 0;

  remainingNodeCount = AssociationGetSinglecastNodeCount();
  if (remainingNodeCount < 2) {
    return MULTICAST_TXRESULT_NOT_ENOUGH_DESTINATIONS;
  }
  else if (m_pAppHandle->pNetworkInfo->SecurityKeys & SECURITY_KEY_S2_MASK) {
    // We got more than one s2 node => Transmit S2 multicast.    
    FramePackage.uTransmitParams.SendDataMultiEx.GroupId = p_nodelist_hold->S2_groupID;
    FramePackage.uTransmitParams.SendDataMultiEx.SourceNodeId = 0xFF;
    FramePackage.uTransmitParams.SendDataMultiEx.eKeyType = GetHighestSecureLevel(m_pAppHandle->pNetworkInfo->SecurityKeys);

    txSecOptions = S2_TXOPTION_SINGLECAST_FOLLOWUP;

    FramePackage.eTransmitType = EZWAVETRANSMITTYPE_MULTI_EX;

    // Put the package on queue (and dont wait for it)
    if (QueueNotifyingSendToBack(m_pAppHandle->pZwTxQueue, (uint8_t*)&FramePackage, 0) != EQUEUENOTIFYING_STATUS_SUCCESS)
    {
      return MULTICAST_TXRESULT_FAILURE;
    }
  } else if (!(m_pAppHandle->pNetworkInfo->SecurityKeys & SECURITY_KEY_NONE_MASK)) {
    for (uint32_t i = 0; i < remainingNodeCount; i++)
    {
      destination_info_t * pNode = AssociationGetSinglecastDestination();
      ZW_NodeMaskSetBit(node_mask, pNode->node.nodeId);
    }

    FramePackage.eTransmitType = EZWAVETRANSMITTYPE_MULTI;
    memcpy(&FramePackage.uTransmitParams.SendDataMulti.NodeMask, &node_mask, sizeof(NODE_MASK_TYPE));

    // Put the package on queue (and dont wait for it)
    if (QueueNotifyingSendToBack(m_pAppHandle->pZwTxQueue, (uint8_t*)&FramePackage, 0) != EQUEUENOTIFYING_STATUS_SUCCESS)
    {
      return MULTICAST_TXRESULT_FAILURE;
    }
  }
  else
  {
    // Do nothing
  }
  return MULTICAST_TXRESULT_SUCCESS;
}

enum ETRANSPORT_MULTICAST_STATUS
ZW_TransportMulticast_SendRequest(const uint8_t * const p_data,
                                  uint8_t data_length,
                                  uint8_t fSupervisionEnable,
                                  TRANSMIT_OPTIONS_TYPE_EX * p_nodelist,
                                  ZAF_TX_Callback_t p_callback)
{
  if (IS_NULL(p_nodelist) || 0 == p_nodelist->list_length || IS_NULL(p_data) || 0 == data_length)
  {
    return ETRANSPORTMULTICAST_FAILED;
  }

  p_callback_hold = p_callback;
  p_nodelist_hold = p_nodelist;
  p_data_hold = (uint8_t *)p_data;
  data_length_hold = data_length;

  if (true != RequestBufferSetPayloadLength((ZW_APPLICATION_TX_BUFFER *)p_data, data_length))
  {
    // Failure to set payload length
    return ETRANSPORTMULTICAST_FAILED;
  }

  // Set the supervision hold variable in order to use it for singlecast
  fSupervisionEnableHold = fSupervisionEnable;

  // Get the active security keys
  secKeys = m_pAppHandle->pNetworkInfo->SecurityKeys;

  // Use supervision if security scheme is 2 and supervision is enabled.
  if (true != RequestBufferSupervisionPayloadActivate((ZW_APPLICATION_TX_BUFFER**)&p_data_hold, &data_length_hold, ((0 != (SECURITY_KEY_S2_MASK & secKeys)) && fSupervisionEnableHold)))
  {
    // Something is wrong.
    return ETRANSPORTMULTICAST_FAILED;
  }

  // The Association Get Destination module must be initialized before getting any destinations.
  AssociationGetDestinationInit(p_nodelist_hold->pList);

  MultiChannelTXResult_t MultiChannelResult = TransmitMultiChannel(NULL);

  if (MCTXRESULT_SUCCESS == MultiChannelResult) {
    return ETRANSPORTMULTICAST_ADDED_TO_QUEUE;
  } else if (MCTXRESULT_FAILURE == MultiChannelResult) {
    return ETRANSPORTMULTICAST_FAILED;
  }

  MulticastTXResult_t MulticastResult = TransmitMultiCast(NULL);

  if (MULTICAST_TXRESULT_SUCCESS == MulticastResult) {
    return ETRANSPORTMULTICAST_ADDED_TO_QUEUE;
  } else if (MULTICAST_TXRESULT_FAILURE == MulticastResult) {
    return ETRANSPORTMULTICAST_FAILED;
  } else {
    /*
    * If there's only one non-endpoint node, we skip the multicast and call
    * the multicast callback directly. This callback will initiate singlecast
    * transmission. The argument can be ignored.
    */
    ZCB_multicast_callback(NULL);
  }

  return ETRANSPORTMULTICAST_ADDED_TO_QUEUE;
}

static void
ZCB_callback_wrapper(uint8_t Status, TX_STATUS_TYPE* pStatusType)
{
  UNUSED(Status);
  UNUSED(pStatusType);
  ZCB_multicast_callback(NULL);
}

/**
 * This function will be called from two sources:
 * 1. when multicast is done OR when there's no need for multicast.
 * 2. when a transmission is done.
 *
 * It will initiate transmission of singlecast frames for each of the nodes
 * included in the multicast.
 */
static void
ZCB_multicast_callback(TRANSMISSION_RESULT * pTransmissionResult)
{
  TRANSMISSION_RESULT_FINISH_STATUS isFinished = TRANSMISSION_RESULT_NOT_FINISHED;
  static TRANSMISSION_RESULT transmissionResult;
  EQueueNotifyingStatus txResult;

  DPRINT("\r\n c");

  if (singlecast_node_count > 0)
  {
    /*
     * When singlecast_node_count is higher than zero, it means that the call
     * to this function is a callback when transmission is done or failed.
     */

    // Check whether to set the finish flag.
    if (singlecast_node_count == remainingNodeCount)
    {
      isFinished = TRANSMISSION_RESULT_FINISHED;
      DPRINT("\r\nTransmission done!");
    }
    else
    {
      isFinished = TRANSMISSION_RESULT_NOT_FINISHED;
    }

    transmissionResult.isFinished = isFinished;
    if (NON_NULL(pTransmissionResult))
    {
      transmissionResult.nodeId = pTransmissionResult->nodeId;
      transmissionResult.status = pTransmissionResult->status;
    }
    else
    {
      transmissionResult.nodeId = 0;
      transmissionResult.status = TRANSMIT_COMPLETE_OK;
    }


    if (NON_NULL(p_callback_hold))
    {
      p_callback_hold(&transmissionResult);
    }
  }

  if (singlecast_node_count < remainingNodeCount)
  {
    TRANSMIT_OPTIONS_TYPE_SINGLE_EX txOptions;

    txOptions.txOptions = p_nodelist_hold->txOptions;
    txOptions.txSecOptions = txSecOptions;
    if(0 == singlecast_node_count && (txSecOptions & S2_TXOPTION_SINGLECAST_FOLLOWUP) )
    {
      txOptions.txSecOptions |= S2_TXOPTION_FIRST_SINGLECAST_FOLLOWUP;
    }

    txOptions.sourceEndpoint = p_nodelist_hold->sourceEndpoint;
    txOptions.pDestNode = AssociationGetSinglecastDestination();
    transmissionResult.nodeId = txOptions.pDestNode->node.nodeId;

    if (true != RequestBufferSupervisionPayloadActivate((ZW_APPLICATION_TX_BUFFER**)&p_data_hold, &data_length_hold, ((0 != (SECURITY_KEY_S2_MASK & secKeys)) && fSupervisionEnableHold && (0 == txOptions.pDestNode->node.BitAddress))))
    {
      transmissionResult.nodeId = 0;
      transmissionResult.status = TRANSMIT_COMPLETE_FAIL;
      transmissionResult.isFinished = TRANSMISSION_RESULT_FINISHED;

      if (NON_NULL(p_callback_hold))
      {
        p_callback_hold(&transmissionResult);
      }
      return;
    }

    txResult = Transport_SendRequestEP(
                  p_data_hold,
                  data_length_hold,
                  &txOptions,
                  ZCB_multicast_callback);

    if (EQUEUENOTIFYING_STATUS_SUCCESS != txResult)
    {
      DPRINTF("\r\nError: %d", txResult);
      transmissionResult.nodeId = 0;
      transmissionResult.status = TRANSMIT_COMPLETE_FAIL;
      transmissionResult.isFinished = TRANSMISSION_RESULT_FINISHED;

      if (NON_NULL(p_callback_hold))
      {
        p_callback_hold(&transmissionResult);
      }
      return;
    }
    singlecast_node_count++;
  }
}

void
ZW_TransportMulticast_clearTimeout(void)
{
  gotSupervision = true;
  ZCB_multicast_callback(NULL);
}
