#pragma once

/**
 * \file console.h
 * \brief Header file for console interface
 */

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>

#include <libopencm3/stm32/usart.h>

#define USART_CONSOLE USART3

void console_setup(void);
void console_send_blocking(const char);
