#pragma once

/**
 * \file console.h
 * \brief Header file for console interface
 *
 * \addtogroup console
 * \{
 */

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>

#include <libopencm3/stm32/usart.h>

#define CONSOLE_USART USART3 //!< Which USART peripheral to use for console output
#define CONSOLE_USART_CLOCK RCC_USART3 //!< Clock for the USART peripheral itself

#define CONSOLE_PORT_CLOCK RCC_GPIOD //!< Which clock to enable for the console rx/tx pins

#define CONSOLE_PORT GPIOD //!< The GPIO Port used for the console's data pins
#define CONSOLE_TX_PIN GPIO8 //!< The GPIO pin used for serial TX
#define CONSOLE_RX_PIN GPIO9 //!< The GPIO pin used for serial RX
#define CONSOLE_AFNO GPIO_AF7 //!< Which alternate function to use for this port

#define CONSOLE_BAUD 115200 //!< Baud rate of the serial console

void console_setup(void);
void console_send_blocking(const char);

/** \} */ // End doxygen group
