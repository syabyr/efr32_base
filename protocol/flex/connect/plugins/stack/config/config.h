/***************************************************************************//**
 * @file
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

/** @addtogroup version
 * @brief Macros to determine the stack version.
 *
 * Note that the Connect Stack version might not match the version of Flex SDK.
 *
 * See config.h for source code.
 *
 * @{
 */

/**
 * @brief The major version of the release. First digit of A.B.C.D.
 */
#define EMBER_MAJOR_VERSION   2

/**
 * @brief The minor version of the release. Second digit of A.B.C.D
 */
#define EMBER_MINOR_VERSION   7

/**
 * @brief The patch version of the release. Third digit of A.B.C.D
 *
 * Patch versions are fully backwards compatible as long as the major and minor
 * version matches.
 */
#define EMBER_PATCH_VERSION   3

/**
 * @brief Special version of the release. Fourth digit of A.B.C.D
 */
#define EMBER_SPECIAL_VERSION 0

/**
 * @brief Build number of the release. Should be stored on 2 bytes.
 */
#define EMBER_BUILD_NUMBER   260

/**
 * @brief Full version number stored on 2 bytes, with each of the four digits
 * stored on 4 bits.
 */
#define EMBER_FULL_VERSION (  ((uint16_t)EMBER_MAJOR_VERSION << 12)   \
                              | ((uint16_t)EMBER_MINOR_VERSION <<  8) \
                              | ((uint16_t)EMBER_PATCH_VERSION <<  4) \
                              | ((uint16_t)EMBER_SPECIAL_VERSION))

/**
 * @brief Version type of the release. EMBER_VERSION_TYPE_GA means
 * generally available.
 */
#define EMBER_VERSION_TYPE EMBER_VERSION_TYPE_GA

/**
 * @copybrief EMBER_FULL_VERSION
 */
#define SOFTWARE_VERSION EMBER_FULL_VERSION

/** @} // End group
 */
