#ifndef MBEDTLS_CONFIG_H
#define MBEDTLS_CONFIG_H

#include <stddef.h>
#include <stdint.h>
#include "em_device.h"

void * sl_calloc(size_t nmemb, size_t size);
void sl_free(void * ptr);

/**
 * \def MBEDTLS_ENTROPY_ADC_C
 *
 * Enable software support for the retrieving entropy data from the ADC
 * incorporated on devices from Silicon Labs.
 *
 * Requires ADC_PRESENT && _ADC_SINGLECTRLX_VREFSEL_VENTROPY &&
 *          _SILICON_LABS_32B_SERIES_1
 */
#if defined(ADC_PRESENT)                        \
  && defined(_ADC_SINGLECTRLX_VREFSEL_VENTROPY) \
  && defined(_SILICON_LABS_32B_SERIES_1)
#define MBEDTLS_ENTROPY_ADC_C
#endif

/**
 * \def MBEDTLS_ENTROPY_ADC_INSTANCE
 *
 * Specify which ADC instance shall be used as entropy source.
 *
 * Requires MBEDTLS_ENTROPY_ADC_C
 */
#if defined(MBEDTLS_ENTROPY_ADC_C)
#define MBEDTLS_ENTROPY_ADC_INSTANCE  (0)
#endif

/**
 * \def MBEDTLS_ENTROPY_RAIL_C
 *
 * Enable software support for the retrieving entropy data from the RAIL
 * incorporated on devices from Silicon Labs.
 *
 * Requires _EFR_DEVICE
 */
#if defined(_EFR_DEVICE) && defined(_SILICON_LABS_32B_SERIES_1)
#define MBEDTLS_ENTROPY_RAIL_C
#endif

/**
 * \def MBEDTLS_ENTROPY_HARDWARE_ALT_RAIL
 *
 * Use the radio (RAIL) as default hardware entropy source.
 *
 * Requires MBEDTLS_ENTROPY_RAIL_C && _SILICON_LABS_32B_SERIES_1 &&
 *         !MBEDTLS_TRNG_C
 */
#if defined(MBEDTLS_ENTROPY_RAIL_C) \
  && defined(_SILICON_LABS_32B_SERIES_1) && !defined(MBEDTLS_TRNG_C)
#define MBEDTLS_ENTROPY_HARDWARE_ALT_RAIL
#endif

/**
 * \def MBEDTLS_ENTROPY_HARDWARE_ALT
 *
 * Integrate the provided default entropy source into the mbed
 * TLS entropy infrastructure.
 *
 * Requires MBEDTLS_TRNG_C || MBEDTLS_ENTROPY_HARDWARE_ALT_RAIL || SEMAILBOX
 */
#if defined(MBEDTLS_TRNG_C) || defined(MBEDTLS_ENTROPY_HARDWARE_ALT_RAIL) || defined(SEMAILBOX_PRESENT)
#define MBEDTLS_ENTROPY_HARDWARE_ALT
#endif

#if !defined(NO_CRYPTO_ACCELERATION)
// Common acceleration
#define MBEDTLS_AES_ALT

#if defined(CRYPTO_COUNT) && (CRYPTO_COUNT > 0)
#define MBEDTLS_SHA1_ALT
#define MBEDTLS_SHA256_ALT

#define MBEDTLS_ECP_INTERNAL_ALT
#define ECP_SHORTWEIERSTRASS
#define MBEDTLS_ECP_ADD_MIXED_ALT
#define MBEDTLS_ECP_DOUBLE_JAC_ALT
#define MBEDTLS_ECP_NORMALIZE_JAC_MANY_ALT
#define MBEDTLS_ECP_NORMALIZE_JAC_ALT
#define MBEDTLS_ECP_RANDOMIZE_JAC_ALT
#define CRYPTO_DEVICE_PREEMPTION
#endif

#if defined(SEMAILBOX_PRESENT)
#define MBEDTLS_SHA1_ALT
#define MBEDTLS_SHA1_PROCESS_ALT
#define MBEDTLS_SHA256_ALT
#define MBEDTLS_SHA256_PROCESS_ALT
#define MBEDTLS_SHA512_ALT
#define MBEDTLS_SHA512_PROCESS_ALT

#define MBEDTLS_ECDH_GEN_PUBLIC_ALT
#define MBEDTLS_ECDH_COMPUTE_SHARED_ALT
#define MBEDTLS_ECDSA_GENKEY_ALT
#define MBEDTLS_ECDSA_SIGN_ALT
#define MBEDTLS_ECDSA_VERIFY_ALT

#define MBEDTLS_CCM_ALT
#define MBEDTLS_CMAC_ALT
#endif

// Threading for multiprotocol
#define MBEDTLS_THREADING_ALT
#define MBEDTLS_THREADING_C
#endif
#define MBEDTLS_NO_PLATFORM_ENTROPY
/* mbed TLS modules */
#define MBEDTLS_CIPHER_C
#define MBEDTLS_ECP_C
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECDH_C
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_CMAC_C
#define MBEDTLS_AES_C
#define MBEDTLS_CCM_C
#define MBEDTLS_CIPHER_MODE_CTR
#undef MBEDTLS_FS_IO
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY
#define MBEDTLS_PLATFORM_CALLOC_MACRO  sl_calloc /**< Default allocator macro to use, can be undefined */
#define MBEDTLS_PLATFORM_FREE_MACRO    sl_free /**< Default free macro to use, can be undefined */
#define MBEDTLS_SHA256_C
#define MBEDTLS_CTR_DRBG_C
#define MBEDTLS_ENTROPY_C
/* Save RAM at the expense of ROM */
#define MBEDTLS_AES_ROM_TABLES
#define MBEDTLS_ECP_DP_CURVE25519_ENABLED
/* Save RAM by adjusting to our exact needs */
#define MBEDTLS_ECP_MAX_BITS   256
#define MBEDTLS_MPI_MAX_SIZE    32 // 384 bits is 48 bytes

/*
   Set MBEDTLS_ECP_WINDOW_SIZE to configure
   ECC point multiplication window size, see ecp.h:
   2 = Save RAM at the expense of speed
   3 = Improve speed at the expense of RAM
   4 = Optimize speed at the expense of RAM
 */
#define MBEDTLS_ECP_WINDOW_SIZE        2
#define MBEDTLS_ECP_FIXED_POINT_OPTIM  0

/* Significant speed benefit at the expense of some ROM */
/* Do not define if only enabling curves that have HW support */
// #define MBEDTLS_ECP_NIST_OPTIM

/* Do not compile in SW ECP code if only enabling curves that have hardware support */
#define MBEDTLS_ECP_NO_FALLBACK

/*
 * You should adjust this to the exact number of sources you're using: default
 * is the "mbedtls_platform_entropy_poll" source, but you may want to add other ones.
 * Minimum is 2 for the entropy test suite.
 */
#define MBEDTLS_ENTROPY_MAX_SOURCES 2

#include "mbedtls/check_config.h"

#endif /* MBEDTLS_CONFIG_H */
