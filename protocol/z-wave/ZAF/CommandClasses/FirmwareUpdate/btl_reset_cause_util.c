/**
 * @file
 * Utilities for accessing the Silicon Labs bootloader reset cause.
 * @copyright 2019 Silicon Laboratories Inc.
 */

#include <btl_reset_cause_util.h>
#include <em_device.h>
#include <btl_reset_info.h>
//#define DEBUGPRINT
#include <DebugPrint.h>

static BootloaderResetCause_t * pBootInfo = (BootloaderResetCause_t *)SRAM_BASE;

bool CC_FirmwareUpdate_IsFirstBoot(uint16_t * pResetReason)
{
  static bool isFirstBoot = false;
  DPRINTF("\nBTL rst reason: %x", pBootInfo->reason);

  if (BOOTLOADER_RESET_REASON_GO == pBootInfo->reason)
  {
    // We booted successfully after a firmware update.
    isFirstBoot = true;
  }

  *pResetReason = pBootInfo->reason;

  // Make sure to clear this reason so that we do not trigger the above on every boot.
  pBootInfo->reason = 0xFFFF; // Using a value unknown to the bootloader (btl_reset_info.h)

  return isFirstBoot;
}

void CC_FirmwareUpdate_InvalidateImage(void)
{
  pBootInfo->signature = BOOTLOADER_RESET_SIGNATURE_INVALID;
}
