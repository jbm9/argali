#include "console.h"

/**
 * \file console.c
 * \brief Serial Console driver implementation (Nucleo F413ZH)
 *
 * \defgroup nucleo_f413zh_console Serial console (Nucleo F413ZH)
 * \{
 * \ingroup nucleo_f413zh
 *
 * Tamodevboard supports a very basic serial console out of the box.
 *
 * And by "very simple," we currently mean "It prints stuff to the
 * serial port."
 */


/**
 * \brief Configure all the peripherals needed for the serial console
 */
void console_setup(void) {
  // Enable USART3 clocks
  rcc_periph_clock_enable(CONSOLE_PORT_CLOCK); // USART TX pin
  rcc_periph_clock_enable(CONSOLE_USART_CLOCK);

  // Connect CONSOLE_USART TX/RX pins via AF
  gpio_mode_setup(CONSOLE_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, CONSOLE_TX_PIN);
  gpio_mode_setup(CONSOLE_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, CONSOLE_RX_PIN);
  gpio_set_af(CONSOLE_PORT, CONSOLE_AFNO, CONSOLE_RX_PIN | CONSOLE_TX_PIN);

  // Now actually set up the USART peripheral
  usart_set_baudrate(CONSOLE_USART, CONSOLE_BAUD);
  usart_set_databits(CONSOLE_USART, 8); // We'll leave the 8N1 hard-coded here for now...
  usart_set_stopbits(CONSOLE_USART, USART_STOPBITS_1);
  usart_set_mode(CONSOLE_USART, USART_MODE_TX_RX);
  usart_set_parity(CONSOLE_USART, USART_PARITY_NONE);
  usart_set_flow_control(CONSOLE_USART, USART_FLOWCONTROL_NONE);

  // And ... engage!
  usart_enable(CONSOLE_USART);
}

/**
 * \brief Send a byte to the console, blocking until we can send (not until sent!)
 */
void console_send_blocking(const char c)
{
  usart_send_blocking(CONSOLE_USART, c);
}

/** \} */ // End doxygen group
