/***************************************************************************//**
 * @file
 * @brief CLI for backing up or restoring TC data to unix filesystem.
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

#include "app/framework/include/af.h"
#include "app/framework/util/common.h"
#include "app/framework/util/util.h"
#include "app/util/serial/command-interpreter2.h"
#include "app/framework/plugin/trust-center-backup/trust-center-backup.h"

#include "app/framework/util/af-main.h"

#include <errno.h>

#if defined(EMBER_TEST)
  #define EMBER_AF_PLUGIN_TRUST_CENTER_BACKUP_POSIX_FILE_BACKUP_SUPPORT
#endif

#if defined(EMBER_AF_PLUGIN_TRUST_CENTER_BACKUP_POSIX_FILE_BACKUP_SUPPORT)

// *****************************************************************************
// Globals

// This is passed as an argument to emberCopyStringArgument() which only
// supports 8-bit values.
#define MAX_FILEPATH_LENGTH 255

// *****************************************************************************
// Forward Declarations

static void getFilePathFromCommandLine(uint8_t* result);

// *****************************************************************************
// Functions

void emAfTcExportCommand(void)
{
  uint8_t file[MAX_FILEPATH_LENGTH];
  getFilePathFromCommandLine(file);

  emberAfTrustCenterExportBackupToFile(file);
}

void emAfTcImportCommand(void)
{
  uint8_t file[MAX_FILEPATH_LENGTH];
  getFilePathFromCommandLine(file);

  emberAfTrustCenterImportBackupFromFile(file);
}

static void getFilePathFromCommandLine(uint8_t* result)
{
  uint8_t length = emberCopyStringArgument(0,
                                           result,
                                           MAX_FILEPATH_LENGTH,
                                           false); // leftpad?
  result[length] = '\0';
}

#endif // defined(EMBER_AF_PLUGIN_POSIX_FILE_BACKUP)
