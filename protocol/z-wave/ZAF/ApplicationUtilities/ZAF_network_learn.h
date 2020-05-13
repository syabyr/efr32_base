/**
 * @file
 * @brief Network learn module
 * @copyright 2019 Silicon Laboratories Inc.
 */

#ifndef ZAF_APPLICATIONUTILITIES_ZAF_NETWORK_LEARN_H_
#define ZAF_APPLICATIONUTILITIES_ZAF_NETWORK_LEARN_H_

#include "ZW_basis_api.h"
/**
 * Sets network learn mode state from applications
 *
 * @param bMode
 * @param wakeUpReason
 */
void ZAF_setNetworkLearnMode(E_NETWORK_LEARN_MODE_ACTION bMode, EResetReason_t wakeUpReason);


#endif /* ZAF_APPLICATIONUTILITIES_ZAF_NETWORK_LEARN_H_ */
