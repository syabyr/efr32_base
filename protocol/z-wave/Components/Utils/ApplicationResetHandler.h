/**
 * @file
 * @brief Application reset handler header file
 * @copyright 2019 Silicon Laboratories Inc.
 */

#ifndef APPLICATIONRESETHANDLER_H_
#define APPLICATIONRESETHANDLER_H_

#include <stdint.h>

void ApplicationReset(uint32_t ApplicationAddress, uint32_t ResetReason);

void ApplicationStart(uint32_t ApplicationAddress);

#endif /* APPLICATIONRESETHANDLER_H_ */
