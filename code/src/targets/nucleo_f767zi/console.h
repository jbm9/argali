#pragma once

/**
 * \file console.h
 * \brief Header file for console interface (Nucleo F767ZI)
 *
 * \addtogroup nucleo_f767zi_console
 * \{
 */

#include <string.h>

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>

#include <libopencm3/stm32/usart.h>

#define CONSOLE_USART USART3 //!< Which USART peripheral to use for console output
#define CONSOLE_USART_CLOCK RCC_USART3 //!< Clock for the USART peripheral itself

#define CONSOLE_ISR_NAME usart3_isr  //!< Name of the USART's ISR

#define CONSOLE_PORT_CLOCK RCC_GPIOD //!< Which clock to enable for the console rx/tx pins

#define CONSOLE_PORT GPIOD //!< The GPIO Port used for the console's data pins
#define CONSOLE_TX_PIN GPIO8 //!< The GPIO pin used for serial TX
#define CONSOLE_RX_PIN GPIO9 //!< The GPIO pin used for serial RX
#define CONSOLE_AFNO GPIO_AF7 //!< Which alternate function to use for this port

#define CONSOLE_BAUD 115200 //!< Baud rate of the serial console


#define CONSOLE_DUMP_PIN GPIO5 //!< A write-only console for debug messages
#define CONSOLE_DUMP_USART USART2 //!< The USART attached to our dump pin
#define CONSOLE_DUMP_CLOCK RCC_USART2 //!< The clock for the USART for our dump pin
#define CONSOLE_DUMP_BAUD 1000000 //!< Baud rate of dump console, higher than default

/**
 * Callback pointer for incoming serial commands
 *
 * Character pointer is the start of the incoming line
 * uint32_t is the length of the received command
 * 
 * After this call is done, the buffer is cleared, so save anything
 * you want during the callback.
 */
typedef void (*console_cb)(char *, uint32_t);

typedef struct {
  console_cb cb;   //!< Callback when a line is received
  char *buf;       //!< Buffer passed in to console_setup()
  uint32_t buflen; //!< Length of buffer
  char *tail;   //!< A pointer to the next free position
} console_state_t;

void console_setup(console_cb, char *, uint32_t);
void console_send_blocking(const char);

void console_dump(const uint8_t *, uint16_t);
void console_dumps(const char*, ...);
void console_dump_hex(const uint8_t *, uint16_t);

/** \} */ // End doxygen group
