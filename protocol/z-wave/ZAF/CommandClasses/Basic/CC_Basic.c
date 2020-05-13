/**
 * @file
 * Handler for Command Class Basic.
 * @copyright 2018 Silicon Laboratories Inc.
 */

/****************************************************************************/
/*                              INCLUDE FILES                               */
/****************************************************************************/
#include <ZW_TransportLayer.h>
#include <ZW_TransportEndpoint.h>
#include <ZW_TransportMulticast.h>
#include <ZW_transport_api.h>
#include <CC_Basic.h>
#include "misc.h"
#include <CC_Common.h>
#include <string.h>

//#define DEBUGPRINT
#include "DebugPrint.h"

/****************************************************************************/
/*                      PRIVATE TYPES and DEFINITIONS                       */
/****************************************************************************/

/****************************************************************************/
/*                              PRIVATE DATA                                */
/****************************************************************************/

/****************************************************************************/
/*                              EXPORTED DATA                               */
/****************************************************************************/

/****************************************************************************/
/*                            PRIVATE FUNCTIONS                             */
/****************************************************************************/

received_frame_status_t
CC_Basic_handler(
  RECEIVE_OPTIONS_TYPE_EX *rxOpt,
  ZW_APPLICATION_TX_BUFFER *pCmd,
  uint8_t cmdLength)
{
  UNUSED(cmdLength);
  switch (pCmd->ZW_Common.cmd)
  {
      //Must be ignored to avoid unintentional operation. Cannot be mapped to another command class.
    case BASIC_SET:
      if ((0x63 < pCmd->ZW_BasicSetV2Frame.value) && (0xFF != pCmd->ZW_BasicSetV2Frame.value))
      {
        return RECEIVED_FRAME_STATUS_FAIL;
      }

      e_cmd_handler_return_code_t return_code = CC_Basic_Set_handler(pCmd->ZW_BasicSetFrame.value, rxOpt->destNode.endpoint);
      /* If handler has finished, call TSE */
      if (E_CMD_HANDLER_RETURN_CODE_FAIL == return_code || E_CMD_HANDLER_RETURN_CODE_HANDLED == return_code)
      {
        // Build up new CC data structure
        void* pZAF_TSE_basicData = CC_Basic_prepare_zaf_tse_data(rxOpt);
        if (false == ZAF_TSE_Trigger(CC_Basic_report_stx, pZAF_TSE_basicData, true))
        {
          DPRINTF("%s(): ZAF_TSE_Trigger failed\n", __func__);
        }
      }
      else if (E_CMD_HANDLER_RETURN_CODE_WORKING == return_code)
      {
       /* If the status is working, inform the application so it can make the TSE trigger later on */
        CC_Basic_report_notifyWorking(rxOpt);
      }

      /* Return the handler result */
      if (E_CMD_HANDLER_RETURN_CODE_FAIL == return_code)
      {
        return RECEIVED_FRAME_STATUS_FAIL;
      }
      break;

    case BASIC_GET:
      if (true == Check_not_legal_response_job(rxOpt))
      {
        return RECEIVED_FRAME_STATUS_FAIL;
      }

      ZAF_TRANSPORT_TX_BUFFER  TxBuf;
      ZW_APPLICATION_TX_BUFFER *pTxBuf = &(TxBuf.appTxBuf);
      memset((uint8_t*)pTxBuf, 0, sizeof(ZW_APPLICATION_TX_BUFFER) );

      TRANSMIT_OPTIONS_TYPE_SINGLE_EX *pTxOptionsEx;
      RxToTxOptions(rxOpt, &pTxOptionsEx);
      /* Controller wants the sensor level */
      pTxBuf->ZW_BasicReportFrame.cmdClass = COMMAND_CLASS_BASIC;
      pTxBuf->ZW_BasicReportFrame.cmd = BASIC_REPORT;

      pTxBuf->ZW_BasicReportV2Frame.currentValue =  CC_Basic_GetCurrentValue_handler(pTxOptionsEx->sourceEndpoint);
      pTxBuf->ZW_BasicReportV2Frame.targetValue =  CC_Basic_GetTargetValue_handler(pTxOptionsEx->sourceEndpoint);
      pTxBuf->ZW_BasicReportV2Frame.duration =  CC_Basic_GetDuration_handler(pTxOptionsEx->sourceEndpoint);
      if(EQUEUENOTIFYING_STATUS_SUCCESS != Transport_SendResponseEP(
          (uint8_t *)pTxBuf,
          sizeof(ZW_BASIC_REPORT_V2_FRAME),
          pTxOptionsEx,
          NULL))
      {
        /*Job failed */
        ;
      }
      break;

    default:
      return RECEIVED_FRAME_STATUS_NO_SUPPORT;
  }
  return RECEIVED_FRAME_STATUS_SUCCESS;
}

JOB_STATUS
CC_Basic_Report_tx(
  AGI_PROFILE* pProfile,
  uint8_t sourceEndpoint,
  uint8_t bValue,
  void (*pCbFunc)(TRANSMISSION_RESULT * pTransmissionResult))
{
  CMD_CLASS_GRP cmdGrp = {COMMAND_CLASS_BASIC, BASIC_REPORT};

  return cc_engine_multicast_request(
      pProfile,
      sourceEndpoint,
      &cmdGrp,
      &bValue,
      1,
      true,
      pCbFunc);
}

REGISTER_CC(COMMAND_CLASS_BASIC, BASIC_VERSION_V2, CC_Basic_handler);
