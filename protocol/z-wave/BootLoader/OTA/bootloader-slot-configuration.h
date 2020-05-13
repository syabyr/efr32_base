/**
 * @file
 * Contains memory addresses for the bootloader.
 * @attention This file MUST NOT be changed.
 * @copyright 2019 Silicon Laboratories Inc.
 */
#ifndef __SI_BOOTLOADER_CONFIG__
#define __SI_BOOTLOADER_CONFIG__

#define BTL_PLUGIN_STORAGE_NUM_SLOTS (1)
#define BTL_STORAGE_SLOT_START_ADDRESS 237568U
#define BTL_STORAGE_SLOT_SIZE 237568U
#define BTL_PLUGIN_STORAGE_SLOTS  \
  {\
    {BTL_STORAGE_SLOT_START_ADDRESS, BTL_STORAGE_SLOT_SIZE}, /* Slot 0 */ \
  }\

// Number of slots in bootload list
#define BTL_STORAGE_BOOTLOAD_LIST_LENGTH BTL_PLUGIN_STORAGE_NUM_SLOTS

#endif // __SI_BOOTLOADER_CONFIG__
