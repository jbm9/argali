#include "console.h"

#include <libopencm3/cm3/nvic.h>

/**
 * \file console.c
 * \brief Serial Console driver implementation (Nucleo F767ZI)
 *
 * \defgroup nucleo_f767zi_console Serial console (Nucleo F767ZI)
 * \{
 * \ingroup nucleo_f767zi
 *
 * Tamodevboard supports a very basic serial console out of the box.
 *
 * And by "very simple," we currently mean "It prints stuff to the
 * serial port."
 */


static console_state_t console_state;

/**
 * \brief Configure all the peripherals needed for the serial console
 *
 * \param cb Callback to use when commands are received (NULL for no callback)
 * \param buf Buffer the console will use to store incoming data
 * \param buflen Length of the buffer  
 */
void console_setup(console_cb cb, char *buf, uint32_t buflen) {

  // Set up our state
  memset(&console_state, 0, sizeof(console_state));
  console_state.cb = cb;
  console_state.buf = buf;
  console_state.buflen = buflen;
  console_state.tail = buf;
  
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

  nvic_enable_irq(NVIC_USART3_IRQ);
  
  // And ... engage!
  usart_enable(CONSOLE_USART);
  if (console_state.cb) {
    usart_enable_rx_interrupt(CONSOLE_USART);
  }
  
}

/**
 * \brief Send a byte to the console, blocking until we can send (not until sent!)
 */
void console_send_blocking(const char c)
{
  usart_send_blocking(CONSOLE_USART, c);
}

void CONSOLE_ISR_NAME(void)
{
  char c = USART_RDR(CONSOLE_USART);
  uint32_t len;
  
  *console_state.tail = c;
  console_send_blocking(c);

  console_state.tail++;

  len = console_state.tail-console_state.buf;
  
  if (c == '\n' || len >= console_state.buflen) {
    console_state.cb(console_state.buf, len);
    console_state.tail = console_state.buf;  // Reset pointer
  }
}

/** \} */ // End doxygen group
