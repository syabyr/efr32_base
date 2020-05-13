/**
 * @file
 * Handler for Command Class Multilevel Switch.
 *
 * @copyright 2018 Silicon Laboratories Inc.
 */

/****************************************************************************/
/*                              INCLUDE FILES                               */
/****************************************************************************/

#include <CC_MultilevelSwitch.h>
#include <ZW_basis_api.h>
#include <ZW_TransportLayer.h>
#include "config_app.h"
#include <multilevel_switch.h>
#include <misc.h>
#include <string.h>
#include "DebugPrint.h"
#include <ZAF_TSE.h>

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

received_frame_status_t handleCommandClassMultilevelSwitch(
  RECEIVE_OPTIONS_TYPE_EX *rxOpt,
  ZW_APPLICATION_TX_BUFFER *pCmd,
  uint8_t cmdLength
)
{
  e_cmd_handler_return_code_t return_code;

  DPRINTF("multilevel sw %d\r\n", rxOpt->destNode.endpoint);

  switch (pCmd->ZW_Common.cmd)
  {
    case SWITCH_MULTILEVEL_GET:

      if (true == Check_not_legal_response_job(rxOpt))
      {
        return RECEIVED_FRAME_STATUS_FAIL;
      }

      ZAF_TRANSPORT_TX_BUFFER  TxBuf;
      ZW_APPLICATION_TX_BUFFER *pTxBuf = &(TxBuf.appTxBuf);
      memset((uint8_t*)pTxBuf, 0, sizeof(ZW_APPLICATION_TX_BUFFER) );

      TRANSMIT_OPTIONS_TYPE_SINGLE_EX* pTxOptionsEx;
      RxToTxOptions(rxOpt, &pTxOptionsEx);
      pTxBuf->ZW_SwitchMultilevelReportV4Frame.cmdClass     = COMMAND_CLASS_SWITCH_MULTILEVEL;
      pTxBuf->ZW_SwitchMultilevelReportV4Frame.cmd          = SWITCH_MULTILEVEL_REPORT;
      pTxBuf->ZW_SwitchMultilevelReportV4Frame.currentValue = CC_MultilevelSwitch_GetCurrentValue_handler(rxOpt->destNode.endpoint);
      pTxBuf->ZW_SwitchMultilevelReportV4Frame.targetValue  = GetTargetLevel(rxOpt->destNode.endpoint);
      pTxBuf->ZW_SwitchMultilevelReportV4Frame.duration     = GetCurrentDuration(rxOpt->destNode.endpoint);

      if (EQUEUENOTIFYING_STATUS_SUCCESS != Transport_SendResponseEP(
          (uint8_t *)pTxBuf,
          sizeof(ZW_SWITCH_MULTILEVEL_REPORT_V4_FRAME),
          pTxOptionsEx,
          NULL))
      {
        /*Job failed */
        return RECEIVED_FRAME_STATUS_FAIL;
      }
      return RECEIVED_FRAME_STATUS_SUCCESS;
      break;

    case SWITCH_MULTILEVEL_SET:
      DPRINT("SET CMD\r\n");

      if ((100 <= pCmd->ZW_SwitchMultilevelSetV3Frame.value) &&
          (255 != pCmd->ZW_SwitchMultilevelSetV3Frame.value))
      {
        return RECEIVED_FRAME_STATUS_FAIL;
      }
      if ((3 == cmdLength) || (0xFF == pCmd->ZW_SwitchMultilevelSetV3Frame.dimmingDuration))
      {
        /*
         * Get the application's default dimming duration in two cases:
         * - If the length is 3, the CC version is 1. Hence, duration is not supported.
         * - If the desired dimming duration is 0xFF (see Table 7, SDS13781-4).
         */
        pCmd->ZW_SwitchMultilevelSetV3Frame.dimmingDuration =
            GetFactoryDefaultDimmingDuration(true, rxOpt->destNode.endpoint);
      }

      return_code = CC_MultilevelSwitch_SetValue(pCmd->ZW_SwitchMultilevelSetV3Frame.value,
                                                    pCmd->ZW_SwitchMultilevelSetV3Frame.dimmingDuration,
                                                    rxOpt->destNode.endpoint);

      if (E_CMD_HANDLER_RETURN_CODE_FAIL == return_code || E_CMD_HANDLER_RETURN_CODE_HANDLED == return_code)
      {
        //keep a pointer to Multilevel switch data when tse needs to be called back later ("working" state)
        void* pMultilevelSwitchData = CC_MultilevelSwitch_prepare_zaf_tse_data(rxOpt);

        // It was an instant change, or there was no change at all. Just trigger TSE and exit.
        DPRINTF("Triggering TSE, return_code: %d, timeout: %u sec\n",
                return_code, pCmd->ZW_SwitchMultilevelStartLevelChangeV3Frame.dimmingDuration);
        if (false == ZAF_TSE_Trigger((void *)CC_MultilevelSwitch_report_stx, pMultilevelSwitchData, true))
        {
          DPRINTF("%s(): ZAF_TSE_Trigger failed\n", __func__);
        }
      }
      else if (E_CMD_HANDLER_RETURN_CODE_WORKING == return_code)
      {
        DPRINTF("%s(): Dimming timer started, timeout %u sec\n",
                         __func__, pCmd->ZW_SwitchMultilevelSetV3Frame.dimmingDuration);
        /* If the status is working, inform the application so it can make the TSE trigger later on */
        CC_MultilevelSwitch_report_notifyWorking(rxOpt, pCmd->ZW_SwitchMultilevelSetV3Frame.dimmingDuration);
      }
      if (E_CMD_HANDLER_RETURN_CODE_FAIL == return_code)
      {
        return RECEIVED_FRAME_STATUS_FAIL;
      }
      break;

    case SWITCH_MULTILEVEL_START_LEVEL_CHANGE:
      {
        bool boDimUp = false;
        bool boIgnoreLevel;
        DPRINT("START CMD\r\n");

        boIgnoreLevel = (pCmd->ZW_SwitchMultilevelStartLevelChangeFrame.level & SWITCH_MULTILEVEL_START_LEVEL_CHANGE_LEVEL_IGNORE_START_LEVEL_BIT_MASK);
        if ((!boIgnoreLevel) &&
            (pCmd->ZW_SwitchMultilevelStartLevelChangeFrame.startLevel > 99) &&
            (pCmd->ZW_SwitchMultilevelStartLevelChangeFrame.startLevel != 0xFF))
        {
          return RECEIVED_FRAME_STATUS_SUCCESS;
        }
        StopSwitchDimming(rxOpt->destNode.endpoint);
        if (cmdLength == 6)
        {
          DPRINT("V 3\r\n");

          if (!(pCmd->ZW_SwitchMultilevelStartLevelChangeV3Frame.properties1&
              SWITCH_MULTILEVEL_START_LEVEL_CHANGE_PROPERTIES1_UP_DOWN_MASK_V3))
          {
            /*primary switch Up/Down bit field value are Up*/
            boDimUp = true;
          }
          else if  ((pCmd->ZW_SwitchMultilevelStartLevelChangeV3Frame.properties1 &
              SWITCH_MULTILEVEL_START_LEVEL_CHANGE_PROPERTIES1_UP_DOWN_MASK_V3) > 0x40)
          {
            /*We should ignore the frame if the  up/down primary switch bit field value is either reserved or no up/down motion*/
            DPRINT("No change\r\n");

            return RECEIVED_FRAME_STATUS_SUCCESS;
          }
        }
        else
        {
          if (cmdLength == 4) /*version 1*/
          {
            DPRINT("V 1\r\n");

            /*dimmingDuration is 1 sec when handling start level change version 1*/
            pCmd->ZW_SwitchMultilevelStartLevelChangeV3Frame.dimmingDuration =
                GetFactoryDefaultDimmingDuration(false, rxOpt->destNode.endpoint);
          }
          if (!(pCmd->ZW_SwitchMultilevelStartLevelChangeFrame.level
              & SWITCH_MULTILEVEL_START_LEVEL_CHANGE_LEVEL_UP_DOWN_BIT_MASK))
          {
            boDimUp = true;
          }
        }

        if (pCmd->ZW_SwitchMultilevelStartLevelChangeV3Frame.dimmingDuration == 0xFF)
        {
          /*use the factory default dimming duration if the value is 0xFF*/
          pCmd->ZW_SwitchMultilevelStartLevelChangeV3Frame.dimmingDuration =
              GetFactoryDefaultDimmingDuration(false, rxOpt->destNode.endpoint);
        }

      return_code = HandleStartChangeCmd(pCmd->ZW_SwitchMultilevelStartLevelChangeFrame.startLevel,
                                         boIgnoreLevel,
                                         boDimUp,
                                         pCmd->ZW_SwitchMultilevelStartLevelChangeV3Frame.dimmingDuration,
                                         rxOpt->destNode.endpoint);
      if (E_CMD_HANDLER_RETURN_CODE_HANDLED == return_code || E_CMD_HANDLER_RETURN_CODE_FAIL == return_code)
      {
        /* For Start Level Change, let the app handle when Lifeline should be notified.
         *
         * If level change has successfully started, inform the application so it can make the TSE trigger later on
         * If it was an instant change, or there was no change at all, there might be some level change already in
         * progress, so let the app handle it anyway.
         * If duration is 0, the app will use 10 ms as default timeout to inform the lifeline.
         */
        DPRINTF("START LEVEL CHANGE handled with status %d, duration %u sec\n", return_code,
                pCmd->ZW_SwitchMultilevelStartLevelChangeV3Frame.dimmingDuration);
        CC_MultilevelSwitch_report_notifyWorking(rxOpt,
                                                 pCmd->ZW_SwitchMultilevelStartLevelChangeV3Frame.dimmingDuration);
      }
      if (E_CMD_HANDLER_RETURN_CODE_FAIL == return_code)
      {
        return RECEIVED_FRAME_STATUS_FAIL;
      }
    }
      return RECEIVED_FRAME_STATUS_SUCCESS;
    break;

    case SWITCH_MULTILEVEL_STOP_LEVEL_CHANGE:
      StopSwitchDimming( rxOpt->destNode.endpoint );
      /* Let the app inform the lifeline about the current state, so that any active timer can be canceled */
      CC_MultilevelSwitch_report_notifyWorking(rxOpt, 0);

      return RECEIVED_FRAME_STATUS_SUCCESS;
      break;
    case SWITCH_MULTILEVEL_SUPPORTED_GET_V3:
      if(false == Check_not_legal_response_job(rxOpt))
      {
        ZAF_TRANSPORT_TX_BUFFER  TxBuf;
        ZW_APPLICATION_TX_BUFFER *pTxBuf = &(TxBuf.appTxBuf);
        memset((uint8_t*)pTxBuf, 0, sizeof(ZW_APPLICATION_TX_BUFFER) );

        TRANSMIT_OPTIONS_TYPE_SINGLE_EX* pTxOptionsEx;
        RxToTxOptions(rxOpt, &pTxOptionsEx);

        pTxBuf->ZW_SwitchMultilevelSupportedReportV3Frame.cmdClass = COMMAND_CLASS_SWITCH_MULTILEVEL;
        pTxBuf->ZW_SwitchMultilevelSupportedReportV3Frame.cmd = SWITCH_MULTILEVEL_SUPPORTED_REPORT_V3;
        pTxBuf->ZW_SwitchMultilevelSupportedReportV3Frame.properties1 = CommandClassMultilevelSwitchPrimaryTypeGet( rxOpt->destNode.endpoint );
        pTxBuf->ZW_SwitchMultilevelSupportedReportV3Frame.properties2 = 0x00; /*The secondary switch type is deprecated, thus the value should be not support (0x02)*/

        if(EQUEUENOTIFYING_STATUS_SUCCESS != Transport_SendResponseEP(
            (uint8_t *)pTxBuf,
            sizeof(ZW_SWITCH_MULTILEVEL_SUPPORTED_REPORT_V3_FRAME),
            pTxOptionsEx,
            NULL))
        {
          /*Job failed */
          ;
        }
        return RECEIVED_FRAME_STATUS_SUCCESS;
      }
      return RECEIVED_FRAME_STATUS_FAIL;
      break;

    default:
      return RECEIVED_FRAME_STATUS_NO_SUPPORT;
      break;
  }
  return RECEIVED_FRAME_STATUS_SUCCESS;
}

void CC_MultilevelSwitch_report_stx(
    TRANSMIT_OPTIONS_TYPE_SINGLE_EX txOptions,
    s_CC_multilevelSwitch_data_t* pData)
{
  DPRINTF("* %s() *\n"
      "\ttxOpt.src = %d\n"
      "\ttxOpt.options %#02x\n"
      "\ttxOpt.secOptions %#02x\n",
      __func__, txOptions.sourceEndpoint, txOptions.txOptions, txOptions.txSecOptions);

  /* Prepare payload for report */
  ZAF_TRANSPORT_TX_BUFFER  TxBuf;
  ZW_APPLICATION_TX_BUFFER *pTxBuf = &(TxBuf.appTxBuf);
  memset((uint8_t*)pTxBuf, 0, sizeof(ZW_APPLICATION_TX_BUFFER) );

  pTxBuf->ZW_SwitchMultilevelReportV4Frame.cmdClass     = COMMAND_CLASS_SWITCH_MULTILEVEL;
  pTxBuf->ZW_SwitchMultilevelReportV4Frame.cmd          = SWITCH_MULTILEVEL_REPORT;
  pTxBuf->ZW_SwitchMultilevelReportV4Frame.currentValue =
      CC_MultilevelSwitch_GetCurrentValue_handler(pData->rxOptions.destNode.endpoint);
  pTxBuf->ZW_SwitchMultilevelReportV4Frame.targetValue  = GetTargetLevel(pData->rxOptions.destNode.endpoint);
  pTxBuf->ZW_SwitchMultilevelReportV4Frame.duration     = GetCurrentDuration(pData->rxOptions.destNode.endpoint);

  if (EQUEUENOTIFYING_STATUS_SUCCESS != Transport_SendRequestEP((uint8_t *)pTxBuf,
                                                                sizeof(ZW_SWITCH_MULTILEVEL_REPORT_V4_FRAME),
                                                                &txOptions,
                                                                ZAF_TSE_TXCallback))
  {
    //sending request failed
    DPRINTF("%s(): Transport_SendRequestEP() failed. \n", __func__);
  }
}

REGISTER_CC(COMMAND_CLASS_SWITCH_MULTILEVEL, SWITCH_MULTILEVEL_VERSION_V4, handleCommandClassMultilevelSwitch);
