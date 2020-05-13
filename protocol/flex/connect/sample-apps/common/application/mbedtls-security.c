////////////////////////////////////////////////////////////
#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_PLATFORM_C)
#include "mbedtls/platform.h"
#else
#include <stdio.h>
#include <stdlib.h>
#define mbedtls_printf          printf
#define mbedtls_exit            exit
#define MBEDTLS_EXIT_SUCCESS    EXIT_SUCCESS
#define MBEDTLS_EXIT_FAILURE    EXIT_FAILURE
#endif /* MBEDTLS_PLATFORM_C */

#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/ecdh.h"

#include PLATFORM_HEADER
#include CONFIGURATION_HEADER
#include "stack/include/ember.h"
#include "hal/hal.h"
#include "debug-print/debug-print.h"
#include "app-join.h"

#define mbedtls_printf emberAfAppPrint
// Contexts
unsigned char cli_srv_exchange_out[32];
unsigned char cli_srv_exchange_in[32];
mbedtls_ecdh_context ctx_ecdh;
mbedtls_entropy_context entropy;
mbedtls_ctr_drbg_context ctr_drbg;
const char pers[] = "ecdh";

/**************************************************************************//**
 * Free mbedTLS context free function
 *
 * @param void
 * @returns void
 *
 * A common function to free the mbedTLS context.
 *
 *****************************************************************************/
static void emberAppMbedTlsFreeContext(void)
{
  mbedtls_ecdh_free(&ctx_ecdh);
  mbedtls_ctr_drbg_free(&ctr_drbg);
  mbedtls_entropy_free(&entropy);
}
/**************************************************************************//**
 * Initialisation of mbedTLS context function
 *
 * @param void
 * @returns result of initialisation
 *
 * Warpper function for initialisation of the context and seed random number
 *
 *****************************************************************************/
int emberAppMbedTlsInitContext(void)
{
  int ret = 1;
  mbedtls_ecdh_init(&ctx_ecdh);
  mbedtls_ctr_drbg_init(&ctr_drbg);
  //Initialize random number generation
  mbedtls_printf("  . Seeding the random number generator...");
  mbedtls_entropy_init(&entropy);
  if ((ret = mbedtls_ctr_drbg_seed(&ctr_drbg,
                                   mbedtls_entropy_func,
                                   &entropy,
                                   (const unsigned char *) pers,
                                   sizeof pers)) != 0) {
    mbedtls_printf(" failed\n  ! mbedtls_ctr_drbg_seed returned %d\n", ret);
    emberAppMbedTlsFreeContext();
    return MBEDTLS_EXIT_FAILURE;
  }
  mbedtls_printf(" ok\n");
  return(MBEDTLS_EXIT_SUCCESS);
}
/**************************************************************************//**
 * mbedTLS Key Pair generation wrapper
 *
 * @param void
 * @returns result of initialisation
 *
 * Warpper function to generate key pair
 *
 *****************************************************************************/
int emberAppMbedTlsGenerateKeyPair(void)
{
  int ret = 1;
  int exit_code = MBEDTLS_EXIT_FAILURE;
  //Initialize context and generate keypair
  mbedtls_printf("  . Setting up context...");
  ret = mbedtls_ecp_group_load(&ctx_ecdh.grp, MBEDTLS_ECP_DP_CURVE25519);
  if (ret != 0) {
    mbedtls_printf(" failed\n  ! mbedtls_ecp_group_load returned %d\n", ret);
    goto exit;
  }

  ret = mbedtls_ecdh_gen_public(&ctx_ecdh.grp,
                                &ctx_ecdh.d,
                                &ctx_ecdh.Q,
                                mbedtls_ctr_drbg_random,
                                &ctr_drbg);
  if (ret != 0) {
    mbedtls_printf(" failed\n  ! mbedtls_ecdh_gen_public returned %d\n", ret);
    goto exit;
  }

  ret = mbedtls_mpi_write_binary(&ctx_ecdh.Q.X, cli_srv_exchange_out, 32);
  if (ret != 0) {
    mbedtls_printf(" failed\n  ! mbedtls_mpi_write_binary returned %d\n", ret);
    goto exit;
  }
  mbedtls_printf(" ok\n");

  exit_code = MBEDTLS_EXIT_SUCCESS;
  return(exit_code);
  exit:
  emberAppMbedTlsFreeContext();
  return(exit_code);
}
/**************************************************************************//**
 * mbedTLS shared secret calculation function
 *
 * @param void
 * @returns result of initialisation
 *
 * Warpper function to calculate shared secret from the public and private
 * key pair.
 *
 *****************************************************************************/
int emberAppMbedTlsCalculateSharedSecret(void)
{
  int ret = 1;
  int exit_code = MBEDTLS_EXIT_FAILURE;
  //Read peer's key and generate shared secret
  mbedtls_printf("  . Server reading peer key and computing secret...");
  ret = mbedtls_mpi_lset(&ctx_ecdh.Qp.Z, 1);
  if (ret != 0) {
    mbedtls_printf(" failed\n  ! mbedtls_mpi_lset returned %d\n", ret);
    goto exit;
  }

  ret = mbedtls_mpi_read_binary(&ctx_ecdh.Qp.X, cli_srv_exchange_in, 32);
  if ( ret != 0 ) {
    mbedtls_printf(" failed\n  ! mbedtls_mpi_read_binary returned %d\n", ret);
    goto exit;
  }

  ret = mbedtls_ecdh_compute_shared(&ctx_ecdh.grp,
                                    &ctx_ecdh.z,
                                    &ctx_ecdh.Qp,
                                    &ctx_ecdh.d,
                                    mbedtls_ctr_drbg_random,
                                    &ctr_drbg);
  if (ret != 0) {
    mbedtls_printf(" failed\n  ! mbedtls_ecdh_compute_shared returned %d\n", ret);
    goto exit;
  }
  mbedtls_printf(" ok\n");

  uint8_t negotiatedKey[32] = { 0 };
  //mbedtls_printf(" Shared Secrete : Sign =%d Number = %d \n", ctx_ecdh.z.s, ctx_ecdh.z.n);
  //for (int i = 0; i < ctx_ecdh.z.n; i++) {
  //  mbedtls_printf(" [%d] ", ctx_ecdh.z.p[i]);
  //}
  ret = mbedtls_mpi_write_binary(&ctx_ecdh.z, negotiatedKey, 32);
  emberAppMbedtlsKeyExchangeNegotiatedKey(negotiatedKey, 32);
  //mbedtls_printf(" ok\n");
  exit_code = MBEDTLS_EXIT_SUCCESS;
  exit:
  emberAppMbedTlsFreeContext();
  return(exit_code);
}
