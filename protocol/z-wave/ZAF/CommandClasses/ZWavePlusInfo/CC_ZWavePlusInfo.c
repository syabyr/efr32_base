/***************************************************************************
*
* @copyright 2018 Silicon Laboratories Inc.
* @brief ZWave+ Info Command Class source file
*/

/****************************************************************************/
/*                              INCLUDE FILES                               */
/****************************************************************************/

#include <CC_ZWavePlusInfo.h>
#include <ZW_plus_version.h>
#include <string.h>
#include <ZW_TransportEndpoint.h>
#include "Assert.h"

/****************************************************************************/
/*                      PRIVATE TYPES and DEFINITIONS                       */
/****************************************************************************/

// Nothing here.

/****************************************************************************/
/*                              PRIVATE DATA                                */
/****************************************************************************/

static SCCZWavePlusInfo const * pData = NULL;

/****************************************************************************/
/*                              EXPORTED DATA                               */
/****************************************************************************/

// Nothing here.

/****************************************************************************/
/*                            PRIVATE FUNCTIONS                             */
/****************************************************************************/

void CC_ZWavePlusInfo_Init(SCCZWavePlusInfo const * const pZWPlusInfo)
{
  pData = pZWPlusInfo;
}

received_frame_status_t
handleCommandClassZWavePlusInfo(
    RECEIVE_OPTIONS_TYPE_EX *rxOpt,
    ZW_APPLICATION_TX_BUFFER *pCmd,
    uint8_t cmdLength
)
{
  UNUSED(cmdLength);

  if (pCmd->ZW_Common.cmd == ZWAVEPLUS_INFO_GET)
  {
    /*Check pTxBuf is free*/
    if(false == Check_not_legal_response_job(rxOpt))
    {
      // Assert if pData has not been set. Otherwise, the following code will try to read from NULL.
      ASSERT(NULL != pData);

      ZAF_TRANSPORT_TX_BUFFER  TxBuf;
      ZW_APPLICATION_TX_BUFFER *pTxBuf = &(TxBuf.appTxBuf);
      memset((uint8_t*)pTxBuf, 0, sizeof(ZW_APPLICATION_TX_BUFFER) );
      ZW_ZWAVEPLUS_INFO_REPORT_V2_FRAME * pFrame = &pTxBuf->ZW_ZwaveplusInfoReportV2Frame;

      TRANSMIT_OPTIONS_TYPE_SINGLE_EX *pTxOptionsEx;
      RxToTxOptions(rxOpt, &pTxOptionsEx);
      pFrame->cmdClass = COMMAND_CLASS_ZWAVEPLUS_INFO;
      pFrame->cmd = ZWAVEPLUS_INFO_REPORT;
      pFrame->zWaveVersion = ZW_PLUS_VERSION;
      pFrame->roleType = pData->roleType;
      pFrame->nodeType = pData->nodeType;

      if((NULL != pData->pEndpointIconList) && // Must be the first check so that we avoid reading from a NULL pointer.
         (NULL != pData->pEndpointIconList->pEndpointInfo) &&
         (pData->pEndpointIconList->endpointInfoSize >= rxOpt->destNode.endpoint) &&
         (0 != rxOpt->destNode.endpoint))
      {
        SEndpointIcon const * const pEndpointInfo = &pData->pEndpointIconList->pEndpointInfo[rxOpt->destNode.endpoint-1];
        pFrame->installerIconType1 = (pEndpointInfo->installerIconType >> 8);
        pFrame->installerIconType2 = (pEndpointInfo->installerIconType & 0xff);
        pFrame->userIconType1 = (pEndpointInfo->userIconType >> 8);
        pFrame->userIconType2 = (pEndpointInfo->userIconType & 0xff);
      }
      else
      {
        pFrame->installerIconType1 = (pData->installerIconType >> 8);
        pFrame->installerIconType2 = (pData->installerIconType & 0xff);
        pFrame->userIconType1 = (pData->userIconType >> 8);
        pFrame->userIconType2 = (pData->userIconType & 0xff);
      }

      {
        uint8_t txResult;

        txResult = Transport_SendResponseEP(
                      (uint8_t *)pTxBuf,
                      sizeof(pTxBuf->ZW_ZwaveplusInfoReportV2Frame),
                      pTxOptionsEx,
                      NULL);

        if (EQUEUENOTIFYING_STATUS_SUCCESS != txResult)
        {
          ;
        }
      }
      return RECEIVED_FRAME_STATUS_SUCCESS;
    }
    return RECEIVED_FRAME_STATUS_FAIL;
  }
  return RECEIVED_FRAME_STATUS_NO_SUPPORT;
}

REGISTER_CC(COMMAND_CLASS_ZWAVEPLUS_INFO, ZWAVEPLUS_INFO_VERSION_V2, handleCommandClassZWavePlusInfo);
