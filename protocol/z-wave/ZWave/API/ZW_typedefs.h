/**
 * @file
 * @copyright 2019 Silicon Laboratories Inc.
 */
#ifndef _ZW_TYPEDEFS_H_
#define _ZW_TYPEDEFS_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/****************************************************************************/
/*                              INCLUDE FILES                               */
/****************************************************************************/

/****************************************************************************/
/*                     EXPORTED TYPES and DEFINITIONS                       */
/****************************************************************************/

#ifndef EOF
#define EOF (-1)
#endif

/* Define for making easy and consistent callback definitions */
#define VOID_CALLBACKFUNC(completedFunc)  void (*completedFunc)

/* Safe null pointer check */
#define IS_NULL(x) (NULL == x)
#define NON_NULL(x) (NULL != x)

#define UNUSED(x) (void)x /* Hack to silence warning - Unreferenced local variable */
#define UNUSED_CONST(x) if(x) ; /* Hack to silence warning - Unreferenced const variable */

/* Gecko chips are little endian:
 *   https://www.silabs.com/community/mcu/32-bit/knowledge-base.entry.html/2017/11/08/endianness_of_silabs-xSJt
 *
 * This macro swaps endianness of a uint32_t
 */
#define UIP_HTONL(x) ( ((x >> 24) & 0x000000FF) | ((x >> 8) & 0x0000FF00) | ((x << 8 ) & 0x00FF0000) | ((x << 24) & 0xFF000000) )

/****************************************************************************/
/*                                 MACROS                                   */
/****************************************************************************/

#endif /* _ZW_TYPEDEFS_H_ */
