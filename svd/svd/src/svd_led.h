/**
 * @file svd_led.h
 * Utility functions to manage ledsConfiguration interface.
 */

#ifndef __SVD_LED_H__
#define __SVD_LED_H__

#define LED_SLOW_BLINK 1000
#define LED_FAST_BLINK 100

/** Turns on named led.*/
void led_on(char *led);
/** Turns off named led.*/
void led_off(char *led);
/** Blinks named led.*/
void led_blink(char *led, int period);

#endif /* __SVD_LED_H__ */