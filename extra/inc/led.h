/*
 * led.h
 *
 *  Created on: May 10, 2020
 *      Author: mybays
 */

#ifndef INC_LED_H_
#define INC_LED_H_

#define LED_PORT gpioPortB
#define LED_PIN	13



void initLed();
void toggleLed();
void ledOn();
void ledOff();


#endif /* INC_LED_H_ */
