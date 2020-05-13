/**
 * @file
 * Command Class Door Lock source file
 * @copyright 2019 Silicon Laboratories Inc.
 */
/****************************************************************************/
/*                              INCLUDE FILES                               */
/****************************************************************************/
#include <CC_DoorLock.h>
#include <ZW_TransportMulticast.h>
#include <string.h>
#include <SwTimer.h>

//#define DEBUGPRINT
#include "DebugPrint.h"
#include <ZAF_TSE.h>
/****************************************************************************/
/*                      PRIVATE TYPES and DEFINITIONS                       */
/****************************************************************************/
#define SUPPORTED_OPERATIONS_LENGTH_BITMASK 0x1F  /**< Bitmask for Supported Opertation Type Length (5 bits)*/


/* private function prototypes */
static void prepare_operation_report(ZW_APPLICATION_TX_BUFFER *pTxBuffer);
static void prepare_configuration_report(ZW_APPLICATION_TX_BUFFER *pTxBuffer);

/****************************************************************************/
/*                              PRIVATE DATA                                */
/****************************************************************************/

/****************************************************************************/
/*                              EXPORTED DATA                               */
/****************************************************************************/

/****************************************************************************/
/*                            PRIVATE FUNCTIONS                             */
/****************************************************************************/

received_frame_status_t CC_DoorLock_handler(
  RECEIVE_OPTIONS_TYPE_EX *rxOpt,
  ZW_APPLICATION_TX_BUFFER *pCmd,
  uint8_t cmdLength)
{
  ZAF_TRANSPORT_TX_BUFFER  TxBuf;
  ZW_APPLICATION_TX_BUFFER *pTxBuf = &(TxBuf.appTxBuf);
  TRANSMIT_OPTIONS_TYPE_SINGLE_EX *pTxOptionsEx;
  UNUSED(cmdLength);
  e_cmd_handler_return_code_t return_code;
  switch (pCmd->ZW_Common.cmd)
  {

    case DOOR_LOCK_OPERATION_SET_V4:
      return_code = CC_DoorLock_OperationSet_handler(pCmd->ZW_DoorLockOperationSetV2Frame.doorLockMode);

      /* If handler has finished, call TSE */
      if (E_CMD_HANDLER_RETURN_CODE_FAIL == return_code || E_CMD_HANDLER_RETURN_CODE_HANDLED == return_code)
      {
        // Build up new CC data structure
        void* pZAF_TSE_doorLockData = CC_Doorlock_prepare_zaf_tse_data(rxOpt);
        if (false == ZAF_TSE_Trigger((void *)CC_DoorLock_operation_report_stx, pZAF_TSE_doorLockData, true))
        {
          DPRINTF("%s(): ZAF_TSE_Trigger failed\n", __func__);
        }
      }
      else if (E_CMD_HANDLER_RETURN_CODE_WORKING == return_code)
      {
       /* If the status is working, inform the application so it can make the TSE trigger later on */
        CC_DoorLock_operation_report_notifyWorking(rxOpt);
      }

      if (E_CMD_HANDLER_RETURN_CODE_FAIL == return_code)
      {
        return RECEIVED_FRAME_STATUS_FAIL;
      }
      break;

    case DOOR_LOCK_OPERATION_GET_V4:
      if (true == Check_not_legal_response_job(rxOpt))
      {
        return RECEIVED_FRAME_STATUS_FAIL;
      }

      RxToTxOptions(rxOpt, &pTxOptionsEx);
      prepare_operation_report(pTxBuf);

      if(EQUEUENOTIFYING_STATUS_SUCCESS != Transport_SendResponseEP(
          (uint8_t *)pTxBuf,
          sizeof(ZW_DOOR_LOCK_OPERATION_REPORT_V4_FRAME),
          pTxOptionsEx,
          NULL))
      {
        return RECEIVED_FRAME_STATUS_FAIL;
      }
      break;

    case DOOR_LOCK_CONFIGURATION_SET_V2:
      return_code = CC_DoorLock_ConfigurationSet_handler(
          (cc_door_lock_configuration_t *)&(pCmd->ZW_DoorLockConfigurationSetV2Frame.operationType));

      if(E_CMD_HANDLER_RETURN_CODE_FAIL == return_code || E_CMD_HANDLER_RETURN_CODE_HANDLED == return_code)
      {
        // Build up new CC data structure
        void* pZAF_TSE_doorLockData = CC_Doorlock_prepare_zaf_tse_data(rxOpt);
        if (false == ZAF_TSE_Trigger((void *)CC_DoorLock_configuration_report_stx, pZAF_TSE_doorLockData, true))
        {
          DPRINTF("%s(): ZAF_TSE_Trigger failed\n", __func__);
        }
      }

      if (E_CMD_HANDLER_RETURN_CODE_FAIL == return_code)
      {
        return RECEIVED_FRAME_STATUS_FAIL;
      }
      break;

    case DOOR_LOCK_CONFIGURATION_GET_V2:
      if (true == Check_not_legal_response_job(rxOpt))
      {
        return RECEIVED_FRAME_STATUS_FAIL;
      }

      RxToTxOptions(rxOpt, &pTxOptionsEx);
      prepare_configuration_report(pTxBuf);

      if(EQUEUENOTIFYING_STATUS_SUCCESS != Transport_SendResponseEP(
          (uint8_t *)pTxBuf,
          sizeof(ZW_DOOR_LOCK_CONFIGURATION_REPORT_V4_FRAME),
          pTxOptionsEx,
          NULL))
      {
        return RECEIVED_FRAME_STATUS_FAIL;
      }
      break;

    case DOOR_LOCK_CAPABILITIES_GET_V4:
      if (true == Check_not_legal_response_job(rxOpt))
      {
        return RECEIVED_FRAME_STATUS_FAIL;
      }
      memset((uint8_t*)pTxBuf, 0, sizeof(ZW_APPLICATION_TX_BUFFER) );

      RxToTxOptions(rxOpt, &pTxOptionsEx);
      pTxBuf->ZW_DoorLockCapabilitiesReport1byteV4Frame.cmdClass = COMMAND_CLASS_DOOR_LOCK_V4;
      pTxBuf->ZW_DoorLockCapabilitiesReport1byteV4Frame.cmd = DOOR_LOCK_CAPABILITIES_REPORT_V4;
      cc_door_lock_capabilities_report_t capabilities_report;
      memset((uint8_t *)&capabilities_report, 0x00, sizeof(capabilities_report));
      CC_DoorLock_CapabilitiesGet_handler(&capabilities_report);
      pTxBuf->ZW_DoorLockCapabilitiesReport1byteV4Frame.properties1 = 1 & SUPPORTED_OPERATIONS_LENGTH_BITMASK; // Length fixed to 1
      uint8_t *ptr = &pTxBuf->ZW_DoorLockCapabilitiesReport1byteV4Frame.supportedOperationTypeBitMask1;
      *ptr++ = capabilities_report.supportedOperationTypeBitmask;
      *ptr++ = capabilities_report.lengthSupportedDoorLockModeList;
      for (unsigned int i = 0; i < capabilities_report.lengthSupportedDoorLockModeList; i++)
      {
        *ptr++ = capabilities_report.supportedDoorLockModeList[i];
      }
      *ptr++ = (capabilities_report.supportedOutsideHandleModes << 4) | capabilities_report.supportedInsideHandleModes;
      *ptr++ = capabilities_report.supportedDoorComponents;
      *ptr++ = (capabilities_report.autoRelockSupport << 3)
               | (capabilities_report.holdAndReleaseSupport << 2)
               | (capabilities_report.twistAssistSupport << 1)
               | capabilities_report.blockToBlockSupport;
      uint8_t send_length = ptr - (uint8_t*)pTxBuf;
      if(EQUEUENOTIFYING_STATUS_SUCCESS != Transport_SendResponseEP(
          (uint8_t *)pTxBuf,
          send_length,
          pTxOptionsEx,
          NULL))
      {
        return RECEIVED_FRAME_STATUS_FAIL;
      }
      break;
    default:
      return RECEIVED_FRAME_STATUS_NO_SUPPORT;
  }
  return RECEIVED_FRAME_STATUS_SUCCESS;
}

JOB_STATUS CC_DoorLock_OperationReport_tx(
  AGI_PROFILE* pProfile,
  uint8_t sourceEndpoint,
  cc_door_lock_operation_report_t* pData,
  void(*pCallback)(TRANSMISSION_RESULT * pTransmissionResult))
{
  CMD_CLASS_GRP cmdGrp = {COMMAND_CLASS_DOOR_LOCK_V2, DOOR_LOCK_OPERATION_REPORT_V2};

  return cc_engine_multicast_request(
      pProfile,
      sourceEndpoint,
      &cmdGrp,
      (uint8_t*)pData,
      sizeof(cc_door_lock_operation_report_t),
      true,
      pCallback);
}

void CC_DoorLock_operation_report_stx(TRANSMIT_OPTIONS_TYPE_SINGLE_EX txOptions, s_CC_doorLock_data_t* pData)
{
  DPRINTF("* %s() *\n"
      "\ttxOpt.src = %d\n"
      "\ttxOpt.options %#02x\n"
      "\ttxOpt.secOptions %#02x\n",
      __func__, txOptions.sourceEndpoint, txOptions.txOptions, txOptions.txSecOptions);

  /* Prepare payload for report */
  ZAF_TRANSPORT_TX_BUFFER  TxBuf;
  ZW_APPLICATION_TX_BUFFER *pTxBuf = &(TxBuf.appTxBuf);
  prepare_operation_report(pTxBuf);

  if (EQUEUENOTIFYING_STATUS_SUCCESS != Transport_SendRequestEP((uint8_t *)pTxBuf,
                                                                sizeof(ZW_DOOR_LOCK_OPERATION_REPORT_V4_FRAME),
                                                                &txOptions,
                                                                ZAF_TSE_TXCallback))
  {
    //sending request failed
    DPRINTF("%s(): Transport_SendRequestEP() failed. \n", __func__);
  }
}

void CC_DoorLock_configuration_report_stx(TRANSMIT_OPTIONS_TYPE_SINGLE_EX txOptions, s_CC_doorLock_data_t* pData)
{
  DPRINTF("* %s() *\n"
      "\ttxOpt.src = %d\n"
      "\ttxOpt.options %#02x\n"
      "\ttxOpt.secOptions %#02x\n",
      __func__, txOptions.sourceEndpoint, txOptions.txOptions, txOptions.txSecOptions);

  /* Prepare payload for report */
  ZAF_TRANSPORT_TX_BUFFER  TxBuf;
  ZW_APPLICATION_TX_BUFFER *pTxBuf = &(TxBuf.appTxBuf);
  prepare_configuration_report(pTxBuf);

  if (EQUEUENOTIFYING_STATUS_SUCCESS != Transport_SendRequestEP((uint8_t *)pTxBuf,
                                                                sizeof(ZW_DOOR_LOCK_CONFIGURATION_REPORT_V4_FRAME),
                                                                &txOptions,
                                                                ZAF_TSE_TXCallback))
  {
    //sending request failed
    DPRINTF("%s(): Transport_SendRequestEP() failed. \n", __func__);
  }
}

/**
 * Prepares payload for operation report to be sent
 * @param pTxBuffer Output payload
 */
static void prepare_operation_report(ZW_APPLICATION_TX_BUFFER *pTxBuffer)
{
  memset((uint8_t*)pTxBuffer, 0, sizeof(ZW_APPLICATION_TX_BUFFER) );

  pTxBuffer->ZW_DoorLockOperationReportV4Frame.cmdClass = COMMAND_CLASS_DOOR_LOCK_V4;
  pTxBuffer->ZW_DoorLockOperationReportV4Frame.cmd = DOOR_LOCK_OPERATION_REPORT_V4;
  cc_door_lock_operation_report_t operation_report;
  CC_DoorLock_OperationGet_handler(&operation_report);
  pTxBuffer->ZW_DoorLockOperationReportV4Frame.currentDoorLockMode = (uint8_t)operation_report.mode;
  pTxBuffer->ZW_DoorLockOperationReportV4Frame.properties1 =
      (operation_report.outsideDoorHandleMode << 4) | operation_report.insideDoorHandleMode;
  pTxBuffer->ZW_DoorLockOperationReportV4Frame.doorCondition = operation_report.condition;
  pTxBuffer->ZW_DoorLockOperationReportV4Frame.lockTimeoutMinutes = operation_report.lockTimeoutMin;
  pTxBuffer->ZW_DoorLockOperationReportV4Frame.lockTimeoutSeconds = operation_report.lockTimeoutSec;
  pTxBuffer->ZW_DoorLockOperationReportV4Frame.targetDoorLockMode = operation_report.targetMode;
  pTxBuffer->ZW_DoorLockOperationReportV4Frame.duration = operation_report.duration;
}

/**
 * Prepares payload for configuration report to be sent
 * @param pTxBuffer Output payload
 */

static void prepare_configuration_report(ZW_APPLICATION_TX_BUFFER *pTxBuffer)
{
  memset((uint8_t*)pTxBuffer, 0, sizeof(ZW_APPLICATION_TX_BUFFER) );

  pTxBuffer->ZW_DoorLockConfigurationReportV4Frame.cmdClass = COMMAND_CLASS_DOOR_LOCK_V4;
  pTxBuffer->ZW_DoorLockConfigurationReportV4Frame.cmd = DOOR_LOCK_CONFIGURATION_REPORT_V4;
  cc_door_lock_configuration_t configuration;
  CC_DoorLock_ConfigurationGet_handler(&configuration);
  pTxBuffer->ZW_DoorLockConfigurationReportV4Frame.operationType = configuration.type;
  pTxBuffer->ZW_DoorLockConfigurationReportV4Frame.properties1 =
      (configuration.outsideDoorHandleMode << 4) | configuration.insideDoorHandleMode;
  pTxBuffer->ZW_DoorLockConfigurationReportV4Frame.lockTimeoutMinutes = configuration.lockTimeoutMin;
  pTxBuffer->ZW_DoorLockConfigurationReportV4Frame.lockTimeoutSeconds = configuration.lockTimeoutSec;
  pTxBuffer->ZW_DoorLockConfigurationReportV4Frame.autoRelockTime1 = configuration.autoRelockTime1;
  pTxBuffer->ZW_DoorLockConfigurationReportV4Frame.autoRelockTime2 = configuration.autoRelockTime2;
  pTxBuffer->ZW_DoorLockConfigurationReportV4Frame.holdAndReleaseTime1 = configuration.holdAndReleaseTime1;
  pTxBuffer->ZW_DoorLockConfigurationReportV4Frame.holdAndReleaseTime2 = configuration.holdAndReleaseTime2;
  pTxBuffer->ZW_DoorLockConfigurationReportV4Frame.properties2 = configuration.reservedOptionsFlags;
}

REGISTER_CC(COMMAND_CLASS_DOOR_LOCK, DOOR_LOCK_VERSION_V4, CC_DoorLock_handler);
