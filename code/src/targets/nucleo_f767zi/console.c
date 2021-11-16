#include "console.h"
#include "dma.h"

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
 *
 * Relevant documentation:
 * RM0410r4 34 is the USART documentation, p1241.
 *
 * Interrupt list is on p1283
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
  dma_settings_t settings = {
                             .dma = DMA1,
                             .stream = DMA_STREAM1,
                             .channel = DMA_SxCR_CHSEL_4,
                             .priority = DMA_SxCR_PL_LOW,

                             .direction = DMA_SxCR_DIR_PERIPHERAL_TO_MEM,
                             .paddr = (uint32_t)&(USART_RDR(CONSOLE_USART)),
                             .peripheral_size = DMA_SxCR_PSIZE_8BIT,
                             .buf = (uint32_t) buf,
                             .buflen = buflen,
                             .mem_size = DMA_SxCR_MSIZE_8BIT,

                             .circular_mode = 1,
                             .double_buffer = 1,

                             .transfer_complete_interrupt = 1,
                             .enable_irq = 1,
                             .irqn = NVIC_DMA1_STREAM1_IRQ,

                             .enable_stream = 1,
  };

  // Set up our state
  memset(&console_state, 0, sizeof(console_state));
  console_state.cb = cb;
  console_state.buf = buf;
  console_state.buflen = buflen;
  console_state.tail = buf;

  // Configuration process on p1278 for DMA RX

  // Enable our DMA channel

  // Enable USART3 clocks
  rcc_periph_clock_enable(CONSOLE_PORT_CLOCK); // USART TX pin
  rcc_periph_clock_enable(CONSOLE_USART_CLOCK);

  // And the clock for our DMA
  rcc_periph_clock_enable(RCC_DMA1);

  // Connect CONSOLE_USART TX/RX pins via AF
  gpio_mode_setup(CONSOLE_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, CONSOLE_TX_PIN);
  gpio_mode_setup(CONSOLE_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, CONSOLE_RX_PIN);
  gpio_set_af(CONSOLE_PORT, CONSOLE_AFNO, CONSOLE_RX_PIN | CONSOLE_TX_PIN);

  // Sequence laid out on RM0410 p1252
  // Now actually set up the USART peripheral
  //

  // 1. Program the M bits in USART_CR1 to define the word length
  usart_set_databits(CONSOLE_USART, 8); // We'll leave the 8N1 hard-coded here for now...

  // 2. Select the desired baud rate using the baud rate register
  // USART_BRR
  usart_set_baudrate(CONSOLE_USART, CONSOLE_BAUD);
  // (divergence from DS) I'm also putting all other CR1 stuff in here
  usart_set_mode(CONSOLE_USART, USART_MODE_TX_RX);
  usart_set_parity(CONSOLE_USART, USART_PARITY_NONE);

  // 3. Program the number of stop bits in USART_CR2.
  usart_set_stopbits(CONSOLE_USART, USART_STOPBITS_1);

  // (divergence from DS) I'll slip the CR3 stuff in here now
  usart_set_flow_control(CONSOLE_USART, USART_FLOWCONTROL_NONE);

  // 4. Enable the USART by writing the UE bit in USART_CR1 register
  // to 1.
  usart_enable(CONSOLE_USART);

  // 5. Select DMA enable (DMAR) in USART_CR3 if multibuffer
  // communication is to take place. Configure the DMA register as
  // explained in multibuffer communication.
  usart_enable_rx_dma(CONSOLE_USART);
  dma_setup(&settings);

  // 6.  Set the RE bit USART_CR1. This enables the receiver which
  // begins searching for a start bit.
  usart_enable(CONSOLE_USART);

  // Turn on interrupts for error states
  usart_enable_error_interrupt(CONSOLE_USART);
  nvic_enable_irq(NVIC_USART3_IRQ);
}

/**
 * \brief Send a byte to the console, blocking until we can send (not until sent!)
 */
void console_send_blocking(const char c)
{
  usart_send_blocking(CONSOLE_USART, c);
}

/**
 * Force a console callback if there is data in the buffer
 */
void console_trigger(void) {
  uint32_t len = console_state.tail-console_state.buf;
  if (0 == len) return;
  console_state.cb(console_state.buf, len);
  console_state.tail = console_state.buf;  // Reset pointer
}


//////////////////////////////////////////////////////////////////////
// ISRs

/**
 * \brief DMA1 Stream1 ISR: USART RX
 *
 * Handles RX buffers from our console USART.
 */
void dma1_stream1_isr(void) {
  if(dma_get_interrupt_flag(DMA1, DMA_STREAM1, DMA_HISR_TCIF4)) {
    // Clear this flag so we can continue
    dma_clear_interrupt_flags(DMA1, DMA_STREAM1, DMA_HISR_TCIF4);

    char *bufpos = console_state.buf;
    if (dma_get_target(DMA1, DMA_STREAM1)) {
      bufpos += console_state.buflen/2;
    }

    if (console_state.cb)
      console_state.cb((char*)bufpos, console_state.buflen/2);
  }
}

void CONSOLE_ISR_NAME(void)
{
  if (0 && usart_get_flag(CONSOLE_USART, USART_ISR_RXNE)) {
    char c = USART_RDR(CONSOLE_USART);
    uint32_t len;

    *console_state.tail = c;
    console_state.tail++;

    len = console_state.tail-console_state.buf;

    if (c == '\n' || len >= console_state.buflen) {
      console_trigger(); // Go ahead and send data over
    }
  }

  // We sometimes get overruns, we'll just ignore them for now
  if (USART_ISR(CONSOLE_USART) & USART_ISR_ORE) {
    USART_ICR(CONSOLE_USART) |= USART_ICR_ORECF;
  }
}

/** \} */ // End doxygen group
