/***************************************************************************//**
 * @brief
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

#if defined(EMBER_AF_API_DEBUG_PRINT)
#include EMBER_AF_API_DEBUG_PRINT
#endif

#if defined(EMBER_AF_API_COMMAND_INTERPRETER2)
#include EMBER_AF_API_COMMAND_INTERPRETER2
#endif

#include "rtos_bluetooth.h"

// TODO: for some reason, in the generated rtos_gecko.h the max command is not
// included in the enums (in the regular gecko_cmd_def.h they are).
#define LE_GAP_MAX_DISCOVERABLE_MODE   0x04
#define LE_GAP_MAX_CONNECTABLE_MODE    0x03
#define LE_GAP_MAX_DISCOVERY_MODE      0x02

#define MAX_CHAR_SIZE 252

#if defined(EMBER_AF_API_COMMAND_INTERPRETER2)

void emberAfPluginBleHelloCommand(void)
{
  struct gecko_msg_system_hello_rsp_t *rsp;

  rsp = gecko_cmd_system_hello();

  emberAfCorePrintln("BLE hello: %s",
                     (rsp->result == bg_err_success) ? "success" : "error");
}

void emberAfPluginBleGetAddressCommand(void)
{
  struct gecko_msg_system_get_bt_address_rsp_t *rsp;

  rsp = gecko_cmd_system_get_bt_address();

  emberAfCorePrintln("BLE address: [%x %x %x %x %x %x]",
                     rsp->address.addr[5], rsp->address.addr[4],
                     rsp->address.addr[3], rsp->address.addr[2],
                     rsp->address.addr[1], rsp->address.addr[0]);
}

void emberAfPluginBleSetAdvertisementParamsCommand(void)
{
  uint16_t minInterval = emberUnsignedCommandArgument(0);
  uint16_t maxInterval = emberUnsignedCommandArgument(1);
  uint8_t channelMap = emberUnsignedCommandArgument(2);
  struct gecko_msg_le_gap_set_adv_parameters_rsp_t* rsp;

  if (channelMap > 7) {
    emberAfCorePrintln("Invalid params");
    return;
  }

  rsp = gecko_cmd_le_gap_set_adv_parameters(minInterval,
                                            maxInterval,
                                            channelMap);

  if (rsp->result == bg_err_success) {
    emberAfCorePrintln("success");
  } else {
    emberAfCorePrintln("error: 0x%2x", rsp->result);
  }
}

void emberAfPluginBleSetGapModeCommand(void)
{
  uint8_t discoverableMode = emberUnsignedCommandArgument(0);
  uint8_t connectableMode = emberUnsignedCommandArgument(1);
  struct gecko_msg_le_gap_set_mode_rsp_t* rsp;

  if (discoverableMode > LE_GAP_MAX_DISCOVERABLE_MODE
      || connectableMode > LE_GAP_MAX_CONNECTABLE_MODE) {
    emberAfCorePrintln("Invalid params");
    return;
  }

  rsp = gecko_cmd_le_gap_set_mode(discoverableMode, connectableMode);

  if (rsp->result == bg_err_success) {
    emberAfCorePrintln("success");
  } else {
    emberAfCorePrintln("error: 0x%2x", rsp->result);
  }
}

void emberAfPluginBleSetBt5GapModeCommand(void)
{
  uint8_t advertiseHandle = emberUnsignedCommandArgument(0);
  uint8_t discoverableMode = emberUnsignedCommandArgument(1);
  uint8_t connectableMode = emberUnsignedCommandArgument(2);
  uint16_t maxEvents = emberUnsignedCommandArgument(3);
  uint8_t addressType = emberUnsignedCommandArgument(4);
  struct gecko_msg_le_gap_bt5_set_mode_rsp_t* rsp;

  if (discoverableMode > LE_GAP_MAX_DISCOVERABLE_MODE
      || connectableMode > LE_GAP_MAX_CONNECTABLE_MODE) {
    emberAfCorePrintln("Invalid params");
    return;
  }
  rsp = gecko_cmd_le_gap_bt5_set_mode(advertiseHandle, discoverableMode, connectableMode, maxEvents, addressType);

  if (rsp->result == bg_err_success) {
    emberAfCorePrintln("success");
  } else {
    emberAfCorePrintln("error: 0x%2x", rsp->result);
  }
}

void emberAfPluginBleGapDiscoverCommand(void)
{
  uint8_t discoveryMode = emberUnsignedCommandArgument(0);
  struct gecko_msg_le_gap_discover_rsp_t* rsp;

  if (discoveryMode > LE_GAP_MAX_DISCOVERY_MODE) {
    emberAfCorePrintln("Invalid params");
    return;
  }

  rsp = gecko_cmd_le_gap_discover(discoveryMode);

  if (rsp->result == bg_err_success) {
    emberAfCorePrintln("success");
  } else {
    emberAfCorePrintln("error: 0x%2x", rsp->result);
  }
}

void emberAfPluginBleGapConnectionOpenCommand(void)
{
  uint8_t addressType = emberUnsignedCommandArgument(1);
  struct gecko_msg_le_gap_open_rsp_t* rsp;
  bd_addr address;

  emberGetExtendedPanIdArgument(0, (uint8_t *)address.addr);

  // TODO: use enums
  if (addressType > 3 && addressType != 16) {
    emberAfCorePrintln("Invalid params");
    return;
  }

  rsp = gecko_cmd_le_gap_open(address, addressType);

  if (rsp->result == bg_err_success) {
    emberAfCorePrintln("success");
  } else {
    emberAfCorePrintln("error: 0x%2x", rsp->result);
  }
}

void emberAfPluginBleConnectionCloseCommand(void)
{
  uint8_t connectionHandle = emberUnsignedCommandArgument(0);

  struct gecko_msg_le_connection_close_rsp_t* rsp
    = gecko_cmd_le_connection_close(connectionHandle);

  if (rsp->result == bg_err_success) {
    emberAfCorePrintln("success");
  } else {
    emberAfCorePrintln("error: 0x%2x", rsp->result);
  }
}

void emberAfPluginBleGapSetConnectionParamsCommand(void)
{
  uint16_t minInterval = emberUnsignedCommandArgument(0);
  uint16_t maxInterval = emberUnsignedCommandArgument(1);
  uint16_t slaveLatency = emberUnsignedCommandArgument(2);
  uint16_t supervisionTimeout = emberUnsignedCommandArgument(3);
  struct gecko_msg_le_gap_set_conn_parameters_rsp_t* rsp;

  // Min and max interval range: 0x0006 to 0x0c80
  // Slave latency range: 0x0000 to 0x01f4
  // Timeout range: 0x000a to 0x0c80
  if ((minInterval >= 0x0006 && minInterval <= 0x0c80)
      && (maxInterval >= 0x0006 && maxInterval <= 0x0c80)
      && slaveLatency <= 0x01f4
      && (supervisionTimeout >= 0x000a && supervisionTimeout <= 0x0c80)) {
    rsp = gecko_cmd_le_gap_set_conn_parameters(minInterval,
                                               maxInterval,
                                               slaveLatency,
                                               supervisionTimeout);
    if (rsp->result == bg_err_success) {
      emberAfCorePrintln("success");
    } else {
      emberAfCorePrintln("error: 0x%2x", rsp->result);
    }
  } else {
    emberAfCorePrintln("error: invalid params");
  }
}

void emberAfPluginBleConnectionSetParamsCommand(void)
{
  uint16_t connectionHandle = emberUnsignedCommandArgument(0);
  uint16_t minInterval = emberUnsignedCommandArgument(1);
  uint16_t maxInterval = emberUnsignedCommandArgument(2);
  uint16_t slaveLatency = emberUnsignedCommandArgument(3);
  uint16_t supervisionTimeout = emberUnsignedCommandArgument(4);
  struct gecko_msg_le_connection_set_parameters_rsp_t* rsp;

  // Min and max interval range: 0x0006 to 0x0c80
  // Slave latency range: 0x0000 to 0x01f4
  // Timeout range: 0x000a to 0x0c80
  if ((minInterval >= 0x0006 && minInterval <= 0x0c80)
      && (maxInterval >= 0x0006 && maxInterval <= 0x0c80)
      && slaveLatency <= 0x01f4
      && (supervisionTimeout >= 0x000a && supervisionTimeout <= 0x0c80)) {
    rsp = gecko_cmd_le_connection_set_parameters(connectionHandle,
                                                 minInterval,
                                                 maxInterval,
                                                 slaveLatency,
                                                 supervisionTimeout);

    if (rsp->result == bg_err_success) {
      emberAfCorePrintln("success");
    } else {
      emberAfCorePrintln("error: 0x%2x", rsp->result);
    }
  } else {
    emberAfCorePrintln("error: invalid params");
  }
}
void emberAfPluginBleSecurityManagerIncreaseSecurityCommand(void)
{
  uint16_t connectionHandle = emberUnsignedCommandArgument(0);
  struct gecko_msg_sm_increase_security_rsp_t* rsp;

  rsp = gecko_cmd_sm_increase_security(connectionHandle);

  if (rsp->result == bg_err_success) {
    emberAfCorePrintln("success");
  } else {
    emberAfCorePrintln("error: 0x%2x", rsp->result);
  }
}

void emberAfPluginBleSecurityManagerListAllBondingsCommand(void)
{
  struct gecko_msg_sm_list_all_bondings_rsp_t* rsp
    = gecko_cmd_sm_list_all_bondings();

  if (rsp->result == bg_err_success) {
    emberAfCorePrintln("success");
  } else {
    emberAfCorePrintln("error: 0x%2x", rsp->result);
  }
}

void emberAfPluginBleSecurityManagerDeleteBondingCommand(void)
{
  uint8_t bondingHandle = emberUnsignedCommandArgument(0);

  struct gecko_msg_sm_delete_bonding_rsp_t* rsp
    = gecko_cmd_sm_delete_bonding(bondingHandle);

  if (rsp->result == bg_err_success) {
    emberAfCorePrintln("success");
  } else {
    emberAfCorePrintln("error: 0x%2x", rsp->result);
  }
}

void emberAfPluginBleSecurityManagerDeleteAllBondingsCommand(void)
{
  struct gecko_msg_sm_delete_bondings_rsp_t* rsp
    = gecko_cmd_sm_delete_bondings();

  if (rsp->result == bg_err_success) {
    emberAfCorePrintln("success");
  } else {
    emberAfCorePrintln("error: 0x%2x", rsp->result);
  }
}

void emberAfPluginBleSecurityManagerSetBondableModeCommand(void)
{
  uint8_t bondableMode = emberUnsignedCommandArgument(0);

  struct gecko_msg_sm_set_bondable_mode_rsp_t* rsp
    = gecko_cmd_sm_set_bondable_mode(bondableMode);

  if (bondableMode > 1) {
    emberAfCorePrintln("Invalid params");
    return;
  }

  if (rsp->result == bg_err_success) {
    emberAfCorePrintln("success");
  } else {
    emberAfCorePrintln("error: 0x%2x", rsp->result);
  }
}

void emberAfPluginBleGattDiscoverPrimaryServicesCommand(void)
{
  uint8_t connectionHandle = emberUnsignedCommandArgument(0);

  struct gecko_msg_gatt_discover_primary_services_rsp_t* rsp
    = gecko_cmd_gatt_discover_primary_services(connectionHandle);

  if (rsp->result == bg_err_success) {
    emberAfCorePrintln("success");
  } else {
    emberAfCorePrintln("error: 0x%2x", rsp->result);
  }
}

void emberAfPluginBleGattDiscoverCharacteristicsCommand(void)
{
  uint8_t connectionHandle = emberUnsignedCommandArgument(0);
  uint32_t serviceHandle = emberUnsignedCommandArgument(1);

  struct gecko_msg_gatt_discover_characteristics_rsp_t* rsp
    = gecko_cmd_gatt_discover_characteristics(connectionHandle, serviceHandle);

  if (rsp->result == bg_err_success) {
    emberAfCorePrintln("success");
  } else {
    emberAfCorePrintln("error: 0x%2x", rsp->result);
  }
}

void emberAfPluginBleGattSetCharacteristicNotificationCommand(void)
{
  uint8_t connectionHandle = emberUnsignedCommandArgument(0);
  uint16_t charHandle = emberUnsignedCommandArgument(1);
  uint8_t flags = emberUnsignedCommandArgument(2);

  struct gecko_msg_gatt_set_characteristic_notification_rsp_t* rsp =
    gecko_cmd_gatt_set_characteristic_notification(connectionHandle,
                                                   charHandle,
                                                   flags);
  if (rsp->result == bg_err_success) {
    emberAfCorePrintln("success");
  } else {
    emberAfCorePrintln("error: 0x%2x", rsp->result);
  }
}

void emberAfPluginBleGattWriteCharacteristicValueCommand(void)
{
  uint8_t connectionHandle = emberUnsignedCommandArgument(0);
  uint16_t characteristic = emberUnsignedCommandArgument(1);
  uint8_t valueLen = emberUnsignedCommandArgument(2);
  uint8_t valueData[MAX_CHAR_SIZE];
  emberGetStringArgument(3, valueData, MAX_CHAR_SIZE, FALSE);

  struct gecko_msg_gatt_write_characteristic_value_rsp_t* rsp =
    gecko_cmd_gatt_write_characteristic_value(connectionHandle, characteristic, valueLen, valueData);
  if (rsp->result == bg_err_success) {
    emberAfCorePrintln("success");
  } else {
    emberAfCorePrintln("error: 0x%2x", rsp->result);
  }
}

#endif //EMBER_AF_API_COMMAND_INTERPRETER2
