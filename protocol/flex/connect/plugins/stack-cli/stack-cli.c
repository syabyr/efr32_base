/***************************************************************************//**
 * @brief Stack-cli commands.
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
#include EMBER_AF_API_DEBUG_PRINT
#include EMBER_AF_API_COMMAND_INTERPRETER2

void startFrequencyHoppingServerCommand(void)
{
  EmberStatus status = emberFrequencyHoppingStartServer();

  if (status != EMBER_SUCCESS) {
    emberAfCorePrintln("FH Server Failed, 0x%x", status);
  } else {
    emberAfCorePrintln("FH Server Success");
  }
}

void startFrequencyHoppingClientCommand(void)
{
  EmberStatus status;
  EmberNodeId nodeId;
  EmberPanId panId;

  nodeId = emberUnsignedCommandArgument(0);
  panId = emberUnsignedCommandArgument(1);

  status = emberFrequencyHoppingStartClient(nodeId, panId);

  if (status != EMBER_SUCCESS) {
    emberAfCorePrintln("FH Client Failed, 0x%x", status);
  } else {
    emberAfCorePrintln("FH Client Success");
  }
}

void stopFrequencyHoppingCommand(void)
{
  EmberStatus status = emberFrequencyHoppingStop();

  if (status != EMBER_SUCCESS) {
    emberAfCorePrintln("FH Stop Failed, 0x%x", status);
  } else {
    emberAfCorePrintln("FH Stopped");
  }
}
