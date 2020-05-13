/**
 * Provides support for EFR32ZG13P531F512GM48 radio board (BRD4203A)
 *
 * @copyright 2019 Silicon Laboratories Inc.
 */
#ifndef RADIO_BOARD_EFR32ZG13P48_H
#define RADIO_BOARD_EFR32ZG13P48_H

/*************************************************************************/
/* Configure RGB LED                                                     */
/*************************************************************************/

/* Don't use this RGB LED if an extension board has already defined its
 * own RGB LED
 */
#if !defined(RGB1_LABEL)
  #define RGB1_LABEL          "LED100"

  #define RGB1_R_GPIO_PORT    gpioPortD
  #define RGB1_R_GPIO_PIN     10
  #define RGB1_R_ON_VALUE     0

  #define RGB1_G_GPIO_PORT    gpioPortD
  #define RGB1_G_GPIO_PIN     11
  #define RGB1_G_ON_VALUE     0

  #define RGB1_B_GPIO_PORT    gpioPortD
  #define RGB1_B_GPIO_PIN     12
  #define RGB1_B_ON_VALUE     0
#endif


#define VCOM_ENABLE (1)
#define VCOM_ENABLE_PORT gpioPortA
#define VCOM_ENABLE_PIN 5

#endif /* RADIO_BOARD_EFR32ZG13P48_H */
