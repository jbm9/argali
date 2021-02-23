#pragma once
/**
 * \file leds.h
 * \brief Header for LED controller
 */

/**
 * \addtogroup LEDs
 * \{
 */


#ifdef TEST_UNITY
#warning Building without hardware support!

#else
// Don't include the hardware-specific code when running host-side
// unit testing.

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>

#define LED_CLOCK RCC_GPIOB  //!< RCC Register for the lock (all LEDs need to be on one port!)
#define LED_PORT GPIOB       //!< Port all LEDs are connected to

#define GREEN_PORT LED_PORT  //!< GPIO Port the Green LED is connected to
#define BLUE_PORT  LED_PORT  //!< GPIO Port the Blue LED is connected to
#define RED_PORT   LED_PORT  //!< GPIO Port the Red LED is connected to

#define GREEN_PIN GPIO0  //!< GPIO Pin the Green LED is connected to
#define BLUE_PIN  GPIO7  //!< GPIO Pin the Blue LED is connected to
#define RED_PIN  GPIO14  //!< GPIO Pin the Red LED is connected to
#endif


void led_setup(void);
void led_red_toggle(void);
void led_red_on(void);
void led_red_off(void);
void led_blue_toggle(void);
void led_blue_on(void);
void led_blue_off(void);
void led_green_toggle(void);
void led_green_on(void);
void led_green_off(void);

/** \} */ // End doxygen group
