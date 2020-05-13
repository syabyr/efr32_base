/**
 * @file
 * Handler for Command Class Manufacturer Specific.
 * @copyright 2018 Silicon Laboratories Inc.
 */

#ifndef _CC_MANUFACTURERSPECIFIC_H_
#define _CC_MANUFACTURERSPECIFIC_H_

/****************************************************************************/
/*                              INCLUDE FILES                               */
/****************************************************************************/

#include <ZAF_types.h>

/****************************************************************************/
/*                     EXPORTED TYPES and DEFINITIONS                       */
/****************************************************************************/

/**
 * Command class manufacturer specific device Id type
 */
typedef enum
{
  DEVICE_ID_TYPE_OEM,
  DEVICE_ID_TYPE_SERIAL_NUMBER,
  DEVICE_ID_TYPE_PSEUDO_RANDOM,
  NUMBER_OF_DEVICE_ID_TYPES
}
device_id_type_t;

/**
 * Command class manufacturer specific device Id format
 */
typedef enum
{
  DEVICE_ID_FORMAT_UTF_8,
  DEVICE_ID_FORMAT_BINARY,
  NUMBER_OF_DEVICE_ID_FORMATS
}
device_id_format_t;

/****************************************************************************/
/*                              EXPORTED DATA                               */
/****************************************************************************/

// Nothing here.

/****************************************************************************/
/*                           EXPORTED FUNCTIONS                             */
/****************************************************************************/

/**
 * @brief Handler for the Manufacturer Specific command class.
 * @param[in] rxOpt receive options of type RECEIVE_OPTIONS_TYPE_EX
 * @param[in] pCmd Payload from the received frame, the union should be used to access
 * the fields.
 * @param[in] cmdLength Number of command bytes including the command.
 * @return receive frame status.
 */
received_frame_status_t handleCommandClassManufacturerSpecific(
    RECEIVE_OPTIONS_TYPE_EX *rxOpt,
    ZW_APPLICATION_TX_BUFFER *pCmd,
    uint8_t cmdLength);

/**
 * Writes the manufacturer ID, product type ID and product ID into the given variables.
 *
 * This function must be implemented by the application.
 * @param[out] pManufacturerID Pointer to manufacturer ID.
 * @param[out] pProductID Pointer to product ID.
 */
void CC_ManufacturerSpecific_ManufacturerSpecificGet_handler(uint16_t * pManufacturerID,
                                                             uint16_t * pProductID);

/**
 * Writes the device ID and related values into the given variables.
 *
 * This function must be implemented by the application.
 * @param pDeviceIDType Pointer to device ID type.
 * @param pDeviceIDDataFormat Pointer to device ID data format.
 * @param pDeviceIDDataLength Pointer to device ID data length.
 * @param pDeviceIDData Pointer to device ID data.
 */
void CC_ManufacturerSpecific_DeviceSpecificGet_handler(device_id_type_t * pDeviceIDType,
                                                       device_id_format_t * pDeviceIDDataFormat,
                                                       uint8_t * pDeviceIDDataLength,
                                                       uint8_t * pDeviceIDData);

#endif /* _CC_MANUFACTURERSPECIFIC_H_ */
