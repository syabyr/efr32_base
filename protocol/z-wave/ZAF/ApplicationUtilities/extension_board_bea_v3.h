/**
 * Provides support for BEA board V3 (Button Extension Board)
 *
 * @copyright 2018 Silicon Laboratories Inc.
 */

#ifndef EXTENSION_BOARD_BEA_V3_H
#define EXTENSION_BOARD_BEA_V3_H

/*************************************************************************/
/* Configure LEDs                                                        */
/*************************************************************************/

#define LED1_LABEL           "D1"
#define LED1_GPIO_PORT       gpioPortF
#define LED1_GPIO_PIN        4
#define LED1_ON_VALUE        0
#define LED1_LETIM0_OUT0_LOC 28

#define LED2_LABEL           "D2"
#define LED2_GPIO_PORT       gpioPortF
#define LED2_GPIO_PIN        5
#define LED2_ON_VALUE        0
#define LED2_LETIM0_OUT0_LOC 29

#define LED3_LABEL           "D3"
#define LED3_GPIO_PORT       gpioPortA
#define LED3_GPIO_PIN        5
#define LED3_ON_VALUE        0
#define LED3_LETIM0_OUT0_LOC 5

/*************************************************************************/
/* Configure RGB LED                                                     */
/*************************************************************************/

#define RGB1_LABEL          "D4"

#define RGB1_R_GPIO_PORT    gpioPortD
#define RGB1_R_GPIO_PIN     13
#define RGB1_R_ON_VALUE     1

#define RGB1_G_GPIO_PORT    gpioPortD
#define RGB1_G_GPIO_PIN     14
#define RGB1_G_ON_VALUE     1

#define RGB1_B_GPIO_PORT    gpioPortD
#define RGB1_B_GPIO_PIN     15
#define RGB1_B_ON_VALUE     1

/*************************************************************************/
/* Configure push buttons                                                */
/*************************************************************************/

#define PB1_LABEL           "S1"
#define PB1_GPIO_PORT       gpioPortF
#define PB1_GPIO_PIN        6
#define PB1_ON_VALUE        0
#define PB1_CAN_WAKEUP_EM4  false

#define PB2_LABEL           "S2"
#define PB2_GPIO_PORT       gpioPortB
#define PB2_GPIO_PIN        11
#define PB2_ON_VALUE        0
#define PB2_CAN_WAKEUP_EM4  false

#define PB3_LABEL           "S3"
#define PB3_GPIO_PORT       gpioPortB
#define PB3_GPIO_PIN        12
#define PB3_ON_VALUE        0
#define PB3_CAN_WAKEUP_EM4  false

#define PB4_LABEL           "S4"
#define PB4_GPIO_PORT       gpioPortB
#define PB4_GPIO_PIN        13
#define PB4_ON_VALUE        0
#define PB4_CAN_WAKEUP_EM4  true

/*************************************************************************/
/* Configure slider button                                               */
/*************************************************************************/

#define SLIDER1_LABEL          "S6"
#define SLIDER1_GPIO_PORT      gpioPortF
#define SLIDER1_GPIO_PIN       7
#define SLIDER1_ON_VALUE       0
#define SLIDER1_CAN_WAKEUP_EM4 true

/*************************************************************************/
/* Map physical board IO devices to application LEDs and buttons         */
/*************************************************************************/

/* Map application LEDs to board LEDs */
#define APP_LED_INDICATOR BOARD_LED1
#define APP_LED_A         BOARD_LED2
#define APP_LED_B         BOARD_LED3
#define APP_LED_C         BOARD_LED4
#define APP_RGB_R         BOARD_RGB1_R
#define APP_RGB_G         BOARD_RGB1_G
#define APP_RGB_B         BOARD_RGB1_B

/* Mapping application buttons to board buttons */
#define APP_BUTTON_A           BOARD_BUTTON_PB1
#define APP_BUTTON_B           BOARD_BUTTON_PB2
#define APP_BUTTON_C           BOARD_BUTTON_PB3
#define APP_BUTTON_LEARN_RESET BOARD_BUTTON_PB4     // Supports EM4 wakeup
#define APP_SLIDER_A           BOARD_BUTTON_SLIDER1 // Supports EM4 wakeup

/* The next two are identical since only PB4 and SLIDER1 can trigger a wakeup
 * from EM4. PB4 is already allocated to learn/reset
 */
#define APP_WAKEUP_BTN_SLDR    BOARD_BUTTON_SLIDER1 // Use this one when wakeup capability is required and button is preferred to slider
#define APP_WAKEUP_SLDR_BTN    BOARD_BUTTON_SLIDER1 // Use this one when wakeup capability is required and slider is preferred to button

/*************************************************************************/
/* Configure UART1                                                       */
/*************************************************************************/

/* NB: The setting below for UART1 conflicts with LED2_GPIO_PORT/PIN and
 *     SLIDER1_GPIO_PORT/PIN on BRD8029A. No problem with BEA V3, but for
 *     consistency UART1 should be configured the same for all extension
 *     boards.
 */
#define UART1_TX_PORT  gpioPortF
#define UART1_TX_PIN   3
#define UART1_RX_PORT  gpioPortC
#define UART1_RX_PIN   9

#endif /* EXTENSION_BOARD_BEA_V3_H */
