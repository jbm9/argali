/**
 * \file leds.c
 * \brief LED Control implementation (Nucleo F767ZI)
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "leds.h"

/**
 * \defgroup nucleo_f767zi_LEDs LED Management routines (Nucleo F767ZI)
 * \{
 * \ingroup nucleo_f767zi
 *
 * This is the hardware-specific side of the LED management code.  It
 * is currently specialized for the STM32F767ZI Nucleo board:
 *
 * - LD1: green: PB0
 * - LD2: blue:  PB7
 * - LD3: red:  PB14
 */

/**
 * \brief Set up the LEDs for use
 */
void led_setup(void) {
#ifndef TEST_UNITY
  // Turn on the clock we need
  rcc_periph_clock_enable(LED_CLOCK);

  gpio_mode_setup(GREEN_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GREEN_PIN);
  gpio_mode_setup(BLUE_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, BLUE_PIN);
  gpio_mode_setup(RED_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, RED_PIN);
#endif
}

//////////////////////////////////////////////////////////////////////
// Green LED management functions

/**
 * \brief Toggle the green LED
 */
void led_green_toggle(void) {
#ifndef TEST_UNITY
  gpio_toggle(GREEN_PORT, GREEN_PIN);
#endif
}

/**
 * \brief Turn the green LED off
 */
void led_green_off(void) {
#ifndef TEST_UNITY
  gpio_clear(GREEN_PORT, GREEN_PIN);
#endif
}

/**
 * \brief Turn the green LED on
 */
void led_green_on(void) {
#ifndef TEST_UNITY
  gpio_set(GREEN_PORT, GREEN_PIN);  
#endif
}


//////////////////////////////////////////////////////////////////////
// Blue LED management functions

/**
 * \brief Toggle the blue LED
 */
void led_blue_toggle(void) {
#ifndef TEST_UNITY
  gpio_toggle(BLUE_PORT, BLUE_PIN);
#endif
}

/**
 * \brief Turn the blue LED off
 */
void led_blue_off(void) {
#ifndef TEST_UNITY
  gpio_clear(BLUE_PORT, BLUE_PIN);
#endif
}

/**
 * \brief Turn the blue LED on
 */
void led_blue_on(void) {
#ifndef TEST_UNITY
  gpio_set(BLUE_PORT, BLUE_PIN);  
#endif
}


//////////////////////////////////////////////////////////////////////
// Red LED management functions

/**
 * \brief Toggle the red LED
 */
void led_red_toggle(void) {
#ifndef TEST_UNITY
  gpio_toggle(RED_PORT, RED_PIN);
#endif
}

/**
 * \brief Turn the red LED off
 */
void led_red_off(void) {
#ifndef TEST_UNITY
  gpio_clear(RED_PORT, RED_PIN);
#endif
}

/**
 * \brief Turn the red LED on
 */
void led_red_on(void) {
#ifndef TEST_UNITY
  gpio_set(RED_PORT, RED_PIN);  
#endif
}
/** \} */ // Close out doyxgen group
