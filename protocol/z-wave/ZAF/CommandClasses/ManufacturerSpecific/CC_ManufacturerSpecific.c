/**
 * @file
 * Handler for Command Class Manufacturer Specific.
 * @copyright 2018 Silicon Laboratories Inc.
 */

/****************************************************************************/
/*                              INCLUDE FILES                               */
/****************************************************************************/
#include <CC_ManufacturerSpecific.h>
#include <ZW_TransportEndpoint.h>
#include <string.h>
#include <ZW_product_id_enum.h>

/****************************************************************************/
/*                      PRIVATE TYPES and DEFINITIONS                       */
/****************************************************************************/

// Nothing here.

/****************************************************************************/
/*                              PRIVATE DATA                                */
/****************************************************************************/

// Nothing here.

/****************************************************************************/
/*                              EXPORTED DATA                               */
/****************************************************************************/

// Nothing here.

/****************************************************************************/
/*                            PRIVATE FUNCTIONS                             */
/****************************************************************************/

received_frame_status_t
handleCommandClassManufacturerSpecific(
    RECEIVE_OPTIONS_TYPE_EX *rxOpt,
    ZW_APPLICATION_TX_BUFFER *pCmd,
    uint8_t cmdLength
)
{
  ZAF_TRANSPORT_TX_BUFFER  TxBuf;
  ZW_APPLICATION_TX_BUFFER *pTxBuf = &(TxBuf.appTxBuf);
  TRANSMIT_OPTIONS_TYPE_SINGLE_EX *txOptionsEx;
  UNUSED(cmdLength);

  if (true == Check_not_legal_response_job(rxOpt))
  {
    // None of the following commands support endpoint bit addressing.
    return RECEIVED_FRAME_STATUS_FAIL;
  }

  switch(pCmd->ZW_Common.cmd)
  {
    case MANUFACTURER_SPECIFIC_GET_V2:
      memset((uint8_t*)pTxBuf, 0, sizeof(ZW_APPLICATION_TX_BUFFER) );
      RxToTxOptions(rxOpt, &txOptionsEx);
      pTxBuf->ZW_ManufacturerSpecificReportV2Frame.cmdClass = COMMAND_CLASS_MANUFACTURER_SPECIFIC_V2;
      pTxBuf->ZW_ManufacturerSpecificReportV2Frame.cmd      = MANUFACTURER_SPECIFIC_REPORT_V2;

      uint16_t manufacturerID = 0;
      uint16_t productID      = 0;
      CC_ManufacturerSpecific_ManufacturerSpecificGet_handler(&manufacturerID,
                                                              &productID);
      pTxBuf->ZW_ManufacturerSpecificReportV2Frame.manufacturerId1 = (manufacturerID >> 8);
      pTxBuf->ZW_ManufacturerSpecificReportV2Frame.manufacturerId2 = (manufacturerID &  0xFF);
      pTxBuf->ZW_ManufacturerSpecificReportV2Frame.productTypeId1  = (((uint16_t)PRODUCT_TYPE_ID_ZWAVE_PLUS_V2)  >> 8);
      pTxBuf->ZW_ManufacturerSpecificReportV2Frame.productTypeId2  = (((uint16_t)PRODUCT_TYPE_ID_ZWAVE_PLUS_V2)  &  0xFF);
      pTxBuf->ZW_ManufacturerSpecificReportV2Frame.productId1      = (productID      >> 8);
      pTxBuf->ZW_ManufacturerSpecificReportV2Frame.productId2      = (productID      &  0xFF);

      if (EQUEUENOTIFYING_STATUS_SUCCESS != Transport_SendResponseEP(
          (uint8_t *)pTxBuf,
          sizeof(ZW_MANUFACTURER_SPECIFIC_REPORT_V2_FRAME),
          txOptionsEx,
          NULL))
      {
        /*Job failed */
        ;
      }
      return RECEIVED_FRAME_STATUS_SUCCESS;
      break;
    case DEVICE_SPECIFIC_GET_V2:
      memset((uint8_t*)pTxBuf, 0, sizeof(ZW_APPLICATION_TX_BUFFER) );

      ZW_DEVICE_SPECIFIC_REPORT_1BYTE_V2_FRAME * pTxData = &(pTxBuf->ZW_DeviceSpecificReport1byteV2Frame);

      RxToTxOptions(rxOpt, &txOptionsEx);
      pTxData->cmdClass = COMMAND_CLASS_MANUFACTURER_SPECIFIC_V2;
      pTxData->cmd      = DEVICE_SPECIFIC_REPORT_V2;

      device_id_type_t deviceIDType       = NUMBER_OF_DEVICE_ID_TYPES;
      device_id_format_t deviceIDDataFormat = NUMBER_OF_DEVICE_ID_FORMATS;
      uint8_t deviceIDDataLength = 0;
      CC_ManufacturerSpecific_DeviceSpecificGet_handler(&deviceIDType,
                                                        &deviceIDDataFormat,
                                                        &deviceIDDataLength,
                                                        &pTxData->deviceIdData1);
      pTxData->properties1 = deviceIDType & 0x07;
      pTxData->properties2 = (deviceIDDataFormat << 5) & 0xE0;
      pTxData->properties2 |= (deviceIDDataLength & 0x1F);

      if(EQUEUENOTIFYING_STATUS_SUCCESS != Transport_SendResponseEP(
          (uint8_t *)pTxBuf,
          sizeof(ZW_DEVICE_SPECIFIC_REPORT_1BYTE_V2_FRAME) +
            (pTxBuf->ZW_DeviceSpecificReport1byteV2Frame.properties2 & 0x1F) - 1, /*Drag out length field 0x1F*/
          txOptionsEx,
          NULL))
      {
        /*Job failed */
        ;
      }
      return RECEIVED_FRAME_STATUS_SUCCESS;
      break;
  }
  return RECEIVED_FRAME_STATUS_NO_SUPPORT;
}

REGISTER_CC(COMMAND_CLASS_MANUFACTURER_SPECIFIC, MANUFACTURER_SPECIFIC_VERSION_V2, handleCommandClassManufacturerSpecific);
