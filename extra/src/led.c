/*
 * led.c
 *
 *  Created on: May 10, 2020
 *      Author: mybays
 */
#include "em_device.h"
#include "em_gpio.h"

#include "led.h"

void initLed()
{
	GPIO_PinModeSet(LED_PORT, LED_PIN, gpioModePushPull, 0);
}


void toggleLed()
{
	GPIO_PinOutToggle(LED_PORT, LED_PIN);
}

void ledOn()
{
	GPIO_PinOutSet(LED_PORT, LED_PIN);
}

void ledOff()
{
	GPIO_PinOutClear(LED_PORT, LED_PIN);
}
