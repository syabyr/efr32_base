/**
 * @file
 * @brief Configuration file for Switch On/Off sample application.
 * @details This file contains definitions for the Z-Wave+ Framework as well for the sample app.
 *
 * @copyright 2018 Silicon Laboratories Inc.
 */
#ifndef _CONFIG_APP_H_
#define _CONFIG_APP_H_

#include <ZW_product_id_enum.h>
#include <CC_ManufacturerSpecific.h>

/**
 * Application version, which is generated during release of SDK.
 * The application developer can freely alter the version numbers
 * according to his needs.
 */
#define APP_VERSION ZAF_VERSION_MAJOR
#define APP_REVISION ZAF_VERSION_MINOR
#define APP_PATCH ZAF_VERSION_PATCH

/**
 * Defines device generic and specific types
 */
//@ [GENERIC_TYPE_ID]
#define GENERIC_TYPE GENERIC_TYPE_SWITCH_BINARY
#define SPECIFIC_TYPE SPECIFIC_TYPE_NOT_USED
//@ [GENERIC_TYPE_ID]

/**
 * See ZW_basic_api.h for ApplicationNodeInformation field deviceOptionMask
 */
//@ [DEVICE_OPTIONS_MASK_ID]
#define DEVICE_OPTIONS_MASK   APPLICATION_NODEINFO_LISTENING
//@ [DEVICE_OPTIONS_MASK_ID]

/**
 * Defines used to initialize the Z-Wave Plus Info Command Class.
 */
//@ [APP_TYPE_ID]
#define APP_ROLE_TYPE ZWAVEPLUS_INFO_REPORT_ROLE_TYPE_SLAVE_ALWAYS_ON
#define APP_NODE_TYPE ZWAVEPLUS_INFO_REPORT_NODE_TYPE_ZWAVEPLUS_NODE
#define APP_ICON_TYPE ICON_TYPE_GENERIC_ON_OFF_POWER_SWITCH
#define APP_USER_ICON_TYPE ICON_TYPE_GENERIC_ON_OFF_POWER_SWITCH
//@ [APP_TYPE_ID]

/**
 * Defines used to initialize the Manufacturer Specific Command Class.
 */
#define APP_MANUFACTURER_ID     MFG_ID_ZWAVE
#define APP_PRODUCT_TYPE_ID     PRODUCT_TYPE_ID_ZWAVE_PLUS_V2
#define APP_PRODUCT_ID          PRODUCT_ID_SwitchOnOff

#define APP_FIRMWARE_ID         APP_PRODUCT_ID | (APP_PRODUCT_TYPE_ID << 8)

/**
 * Defines used to initialize the Association Group Information (AGI)
 * Command Class.
 */
#define NUMBER_OF_ENDPOINTS         0
#define MAX_ASSOCIATION_GROUPS      1
#define MAX_ASSOCIATION_IN_GROUP    5

/*
 * File identifiers for application file system
 * Range: 0x00000 - 0x0FFFF
 */
#define FILE_ID_APPLICATIONDATA (0x00000)

//@ [AGI_TABLE_ID]
#define AGITABLE_LIFELINE_GROUP \
 {COMMAND_CLASS_DEVICE_RESET_LOCALLY, DEVICE_RESET_LOCALLY_NOTIFICATION}, \
 {COMMAND_CLASS_SWITCH_BINARY_V2, SWITCH_BINARY_REPORT_V2},\
 {COMMAND_CLASS_INDICATOR, INDICATOR_REPORT_V3}


#define  AGITABLE_ROOTDEVICE_GROUPS NULL
//@ [AGI_TABLE_ID]

/**
 * The value basic set command should use  when an event occur
 */
#define BASIC_SET_TRIGGER_VALUE (0xFF)
/**
 * The timeout value in sec that used when sending basic set command due to an event
 */
#define BASIC_SET_TIMEOUT    10

/**
 * Security keys
 */
//@ [REQUESTED_SECURITY_KEYS_ID]
#define REQUESTED_SECURITY_KEYS (SECURITY_KEY_S0_BIT | SECURITY_KEY_S2_UNAUTHENTICATED_BIT | SECURITY_KEY_S2_AUTHENTICATED_BIT)
//@ [REQUESTED_SECURITY_KEYS_ID]

/**
 * Setup the Z-Wave frequency
 *
 * The definition is only set if it's not already set to make it possible to pass the frequency to
 * the compiler by command line or in the Studio project.
 */
#ifndef APP_FREQ
#define APP_FREQ REGION_DEFAULT
#endif

#endif /* _CONFIG_APP_H_ */

