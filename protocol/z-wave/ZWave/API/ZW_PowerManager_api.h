/**
 * @file
 * Contains a number of types used in the API between application and protocol.
 * @copyright 2018 Silicon Laboratories Inc.
 */

#ifndef ZWAVE_API_ZW_POWERMANAGER_API_H_
#define ZWAVE_API_ZW_POWERMANAGER_API_H_

#include <stdint.h>

#define MAX_POWERDOWN_CALLBACKS  4

typedef enum
{
  PM_TYPE_RADIO,       //Prevents system from entering EM2
  PM_TYPE_PERIPHERAL,  //Prevents system from entering EM3
} pm_type_t;

typedef struct SPowerLock_t
{
  uint32_t dummy[6];
} SPowerLock_t;

#endif /* ZWAVE_API_ZW_POWERMANAGER_API_H_ */
