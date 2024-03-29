/***************************************************************************//**
 * @brief Functions for reading and writing command and response frames.
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

#ifndef __CSP_FRAME_UTILITIES_H__
#define __CSP_FRAME_UTILITIES_H__

#define CSP_BUFFER_SIZE 1000

void initializeCspBuffers(void);

uint8_t fetchUint8_t(void);
uint8_t *fetchUint8_tPointer(uint8_t length);
void appendInt8_t(int8_t value);
void appendUint8_t(uint8_t value);
uint16_t fetchUint16_t(void);
void appendUint16_t(uint16_t value);

void appendUint32_t(uint32_t value);
uint32_t fetchUint32_t(void);

void appendUint8_tArray(uint8_t length, uint8_t *contents);
void fetchUint8_tArray(uint8_t length, uint8_t *contents);

void appendUint16_tArray(uint8_t length, uint16_t *contents);
void fetchUint16_tArray(uint8_t length, uint16_t *contents);

uint8_t emberKeyDataCommandArgument(uint8_t argNum, EmberKeyData *key);
uint8_t emberEUI64CommandArgument(uint8_t argNum, EmberEUI64 *eui64);

// Format the same as in command-interpreter2.h, with the addition of
// a 'p' for a pointer.
// Make sure to pass references to all of the variables! And if an array is
// passed, the length needs to come directly after it. This length should
// be a reference as well.
void emAfFetchOrAppend(boolean fetch, PGM_P format, ...);

#endif // __CSP_FRAME_UTILITIES_H__
