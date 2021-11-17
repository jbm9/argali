#include <stdio.h>
#include <stdarg.h>
#include "console.h"
#include "dma.h"

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
 *
 * Relevant documentation is RM0430r8 Chapter 28, p883
 */



static console_state_t console_state;

#define DUMPBUFLEN 512 //!< Size of the buffer for the dump console
static uint8_t dumpbuf[DUMPBUFLEN]; //!< Buffer for the dump console

/**
 * Set up our diagnostic/dumping console
 *
 * This is a write-only debugging aid that should be removed at some
 * point.  Until then, it's a megabaud hole to shovel debug info into.
 */
static void console_dump_setup(void) {
  // Enable our dump port clock
  rcc_periph_clock_enable(CONSOLE_DUMP_CLOCK);

  // And connect our debug dump pin as well
  gpio_mode_setup(CONSOLE_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, CONSOLE_DUMP_PIN);
  gpio_set_af(CONSOLE_PORT, CONSOLE_AFNO, CONSOLE_DUMP_PIN);
  // Now actually set up the USART peripheral, laid out in 28.4.2
  // Receiver's "Character transmission" section, which is on pp890
  // DMA for USART TX is on p919 of RM0430r8, "Transmission using DMA"

  // 1. Enable the USART by writing the UE bit in USART_CR1 register
  // to 1.
  usart_enable(CONSOLE_DUMP_USART);

  // 2. Program the M bit in USART_CR1 to define the word length.
  usart_set_databits(CONSOLE_DUMP_USART, 8); // We'll leave the 8N1 hard-coded here for now...
  // (divergence from DS) I'm also putting all other CR1 stuff in here
  usart_set_parity(CONSOLE_DUMP_USART, USART_PARITY_NONE);

  // 3. Program the number of stop bits in USART_CR2.
  usart_set_stopbits(CONSOLE_DUMP_USART, USART_STOPBITS_1);

  // (divergence from DS) I'll slip the CR3 stuff in here now
  usart_set_flow_control(CONSOLE_DUMP_USART, USART_FLOWCONTROL_NONE);

  // 4. Select DMA enable (DMAT) in USART_CR3 if multibuffer
  // communication is to take place. Configure the DMA register as
  // explained in multibuffer communication.

  // Supposed to DMA here?
  usart_enable_tx_dma(CONSOLE_DUMP_USART);

  // 5. Select the desired baud rate using the baud rate register
  // USART_BRR
  usart_set_baudrate(CONSOLE_DUMP_USART, CONSOLE_DUMP_BAUD);

  // 6. Set the TE bit USART_CR1 to send an idle frame as the first
  // transmission.
  usart_set_mode(CONSOLE_DUMP_USART, USART_MODE_TX);

  usart_enable(CONSOLE_DUMP_USART); // We enable again, per the the
                                    // reception setup process step 6.
}

/**
 * Dump formatted strings to the dump console
 *
 * This is used to effectively printf() to the debug console.
 */
void console_dumps(const char *fmt, ...) {
  uint16_t buflen;
  va_list argp;

  va_start(argp, fmt);
  buflen = vsprintf((char*)dumpbuf, fmt, argp);
  va_end(argp);

  console_dump((const uint8_t*)dumpbuf, buflen);
}

/**
 * Dump a buffer out the dump console as hex
 *
 * \param buf Pointer to buffer of data
 * \param buflen Length to dump out
 *
 * Note that buflen must be less than half of DUMPBUFLEN or you will
 * run out of room.
 */
void console_dump_hex(const uint8_t *buf, uint16_t buflen) {
  for (int i = 0; i < buflen; i++) {
    sprintf((char*)dumpbuf+2*i, "%02x", buf[i]);
  }
  console_dump(dumpbuf, buflen*2);
}

/**
 * Dump a buffer full of data out the dump buffer (DMA'd)
 *
 * \param buf The buffer to dump
 * \param buflen How many bytes to dump
 *
 * This shovels data out the dump console (USART2, pin CN9.6 on the
 * Nucleo 144) at 1Mbaud.  This is a bit over 8x faster than the input
 * on the main console, which allows us a lot of freedom before we
 * overflow the port.
 */
void console_dump(const uint8_t *buf, uint16_t buflen) {
  // Bring up a DMA sender per p919

  if (buflen > DUMPBUFLEN) {
    console_dumps("oversize buffer truncated\n");
    memcpy(dumpbuf, buf, DUMPBUFLEN);
    memcpy(dumpbuf, "OVERSIZE", 8);
  } else {
    memcpy(dumpbuf, buf, buflen);
  }

  dma_settings_t settings = {
                             .dma = DMA1,
                             .stream = DMA_STREAM6,
                             .channel = DMA_SxCR_CHSEL_4,
                             .priority = DMA_SxCR_PL_HIGH,

                             .direction = DMA_SxCR_DIR_MEM_TO_PERIPHERAL,
                             .paddr = (uint32_t)&(USART_DR(CONSOLE_DUMP_USART)),
                             .peripheral_size = DMA_SxCR_PSIZE_8BIT,
                             .buf = (uint32_t) dumpbuf,
                             .buflen = buflen,
                             .mem_size = DMA_SxCR_MSIZE_8BIT,

                             .circular_mode = 0,
                             .double_buffer = 0,

                             .transfer_complete_interrupt = 1,
                             .enable_irq = 0,
                             .irqn = NVIC_DMA1_STREAM6_IRQ,

                             .enable_stream = 1,
  };

  USART_SR(CONSOLE_DUMP_USART) |= USART_SR_TC; // p919 step 6
  dma_setup(&settings);
}

//////////////////////////////////////////////////////////////////////
// Actual console implementation

/**
 * \brief Configure all the peripherals needed for the serial console
 */
void console_setup(console_cb cb, char *buf, uint32_t buflen) {
  dma_settings_t settings = {
                             .dma = DMA1,
                             .stream = DMA_STREAM1,
                             .channel = DMA_SxCR_CHSEL_4,
                             .priority = DMA_SxCR_PL_LOW,

                             .direction = DMA_SxCR_DIR_PERIPHERAL_TO_MEM,
                             .paddr = (uint32_t)&(USART_DR(CONSOLE_USART)),
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

  rcc_periph_clock_enable(CONSOLE_PORT_CLOCK);
  console_dump_setup();

  // Set up our state
  memset(&console_state, 0, sizeof(console_state));
  console_state.cb = cb;
  console_state.buf = buf;
  console_state.buflen = buflen;
  console_state.tail = buf;

  // Enable USART clocks
  rcc_periph_clock_enable(CONSOLE_PORT_CLOCK);
  rcc_periph_clock_enable(CONSOLE_USART_CLOCK);
  rcc_periph_clock_enable(RCC_DMA1);

  // Connect CONSOLE_USART TX/RX pins via AF
  gpio_mode_setup(CONSOLE_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, CONSOLE_TX_PIN);
  gpio_mode_setup(CONSOLE_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, CONSOLE_RX_PIN);
  gpio_set_af(CONSOLE_PORT, CONSOLE_AFNO, CONSOLE_RX_PIN | CONSOLE_TX_PIN);

  // Now actually set up the USART peripheral, laid out in 28.4.3
  // Receiver's "Character reception" section, which is on pp892&3.
  // DMA for USART RX is on p920 of RM0430r8, "Reception using DMA"

  // 1. Enable the USART by writing the UE bit in USART_CR1 register
  // to 1.
  usart_enable(CONSOLE_USART);

  // 2. Program the M bit in USART_CR1 to define the word length.
  usart_set_databits(CONSOLE_USART, 8); // We'll leave the 8N1 hard-coded here for now...
  // (divergence from DS) I'm also putting all other CR1 stuff in here
  usart_set_mode(CONSOLE_USART, USART_MODE_TX_RX);
  usart_set_parity(CONSOLE_USART, USART_PARITY_NONE);

  // 3. Program the number of stop bits in USART_CR2.
  usart_set_stopbits(CONSOLE_USART, USART_STOPBITS_1);

  // (divergence from DS) I'll slip the CR3 stuff in here now
  usart_set_flow_control(CONSOLE_USART, USART_FLOWCONTROL_NONE);

  // 4. Select DMA enable (DMAR) in USART_CR3 if multibuffer
  // communication is to take place. Configure the DMA register as
  // explained in multibuffer communication. STEP 3

  // p920 is the multibuffer bringup guide, and it's just "enable DMA
  // on the USART, then bring up DMA."
  usart_enable_rx_dma(CONSOLE_USART);
  dma_setup(&settings);

  // 5. Select the desired baud rate using the baud rate register
  // USART_BRR
  usart_set_baudrate(CONSOLE_USART, CONSOLE_BAUD);

  // 6. Set the RE bit USART_CR1. This enables the receiver that
  // begins searching for a start abit.
  usart_enable(CONSOLE_USART); // We enable again, per the the
                               // reception setup process step 6.

  // We'll put interrupt stuff at the bottom?  Sure.
  nvic_enable_irq(NVIC_USART3_IRQ);
  usart_enable_error_interrupt(CONSOLE_USART);
}

/**
 * \brief Send a byte to the console, blocking until we can send (not until sent!)
 */
void console_send_blocking(const char c)
{
  usart_send_blocking(CONSOLE_USART, c);
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

    console_dump_hex((const uint8_t*)bufpos, console_state.buflen/2);
  }
}

/**
 * USART interrupt
 *
 * See p923 for list of possible interrupt sources, and the
 * documentation on pp925&6 for all the fields which can trigger this.
 * The implementation here is very sparse in these flags, mostly
 * because I don't have the ability to test them all.
 */
void CONSOLE_ISR_NAME(void)
{

  // We sometimes get overruns, we'll just ignore them for now
  if (USART_SR(CONSOLE_USART) & USART_SR_ORE) { // p926
    // To clear this, you read USART_SR then USART_DR
    char c = USART_DR(CONSOLE_USART);
  }
}

/** \} */ // End doxygen group
