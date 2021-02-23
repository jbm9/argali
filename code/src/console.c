#include "console.h"

/**
 * \file console.c
 * \brief Serial Console driver implementation
 */


/**
 * \brief Configure all the peripherals needed for the serial console
 */
void console_setup(void) {
  // Enable USART3 clocks
  rcc_periph_clock_enable(RCC_GPIOD); // USART TX pin
  rcc_periph_clock_enable(RCC_USART3);

  // Connect USART3 TX/RX pins via AF
  gpio_mode_setup(GPIOD, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO8);
  gpio_mode_setup(GPIOD, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO7);
  gpio_set_af(GPIOD, GPIO_AF7, GPIO8 | GPIO9);

  // Now actually set up the USART peripheral
  usart_set_baudrate(USART3, 115200);
  usart_set_databits(USART3, 8);
  usart_set_stopbits(USART3, USART_STOPBITS_1);
  usart_set_mode(USART3, USART_MODE_TX_RX);
  usart_set_parity(USART3, USART_PARITY_NONE);
  usart_set_flow_control(USART3, USART_FLOWCONTROL_NONE);

  // And ... engage!
  usart_enable(USART3);
}

/**
 * \brief Send a byte to the console, blocking until we can send (not until sent!)
 */
void console_send_blocking(const char c)
{
  usart_send_blocking(USART_CONSOLE, c);
}
