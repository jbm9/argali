#pragma once

#include <stdbool.h>

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>

/**
 * \file buttons.h
 * \brief Header file for buttons driver
 */

/**
 * \addtogroup buttons
 * \{
 */

// usr button: PC13

#define BUTTON_PORT GPIOC //!< GPIO Port the button is attached to
#define BUTTON_PIN GPIO13 //!< GPIO Pin the button is attached to (as a libopencm3 gpio mask value)
#define BUTTON_CLOCK RCC_GPIOC //!< RCC bank to enable for the button

void button_setup(void);
bool button_poll(void);

/** \} */ // End doxygen group
