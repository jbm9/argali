#pragma once

#include <stdbool.h>

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>

/**
 * \file buttons.h
 * \brief Header file for buttons driver
 */


// usr button: PC13

#define BUTTON_PORT GPIOC
#define BUTTON_PIN GPIO13
#define BUTTON_CLOCK RCC_GPIOC

void button_setup(void);
bool button_poll(void);
