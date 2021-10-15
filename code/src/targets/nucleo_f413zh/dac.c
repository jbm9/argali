/**
 * \file dac.c
 * \brief Nucleo F413ZH DAC interface implementation
 */

#include "dac.h"

#include "logging.h"

#include <string.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/dac.h>
#include <libopencm3/stm32/dma.h>

/**
 * \defgroup nucleo_f413zh_dac DAC driver (Nucleo F413ZH)
 *
 * \addtogroup nucleo_f413zh_dac
 * \{
 * \ingroup nucleo_f413zh
 *
 * This is a DMA-driven DAC output driver.
 *
 * Resources needed for the F413ZH's DMA DAC:
 *
 * * DMA channel
 * * Timer (to advance values)
 * * DAC channel
 * * Clock channels
 * * GPIO AF assignment
 *
 * Here's a rough rundown of the workflow to use this driver:
 *
 * Call dac_setup() once at boot to enable all clock domains etc.
 * This is a bit wasteful of power, and will probably want to be
 * tweaked if you want to run on battery power.
 *
 * Then, to actually output data via the DAC:
 *
 * Register your buffer (with its length) via dac_register_buf()
 *
 * Call dac_start() to start the DMA output
 *
 * Call dac_stop() to stop the DMA output
 *
 * Note that the current implementation is very rough around the
 * edges, and is based on the libopencm3 dac-dma example.
 */

static dac_state_t dac_state;


/**
 * \brief Sets up the clock and GPIO pin for our DAC
 */
void dac_setup(void) {
  memset(&dac_state, 0, sizeof(dac_state_t));
  dac_state.initialized = false;  // Not strictly needed, but be explicit.

  // Set up our GPIO
  rcc_periph_clock_enable(DAC_GPIO_CLOCK);
  // Set our pin to HiZ for now
  gpio_mode_setup(DAC_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE, DAC_PIN);

  // Set up our timer, but don't enable it

  // DEBUG Enable PC1 for ISR blinking: P9.A3
  rcc_periph_clock_enable(RCC_GPIOC);
  gpio_mode_setup(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO1);
  //  gpio_set_output_options(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, GPIO1);
}

/**
 * \brief Register a buffer to be used for output
 *
 * \param buf A pointer to the buffer to be DMAd out
 * \param buflen The number of samples in the buffer (64k max for F4)
 *
 * This simply sets up the DMA channel used by the DAC.  It does not
 * actually start the output.
 */
int dac_register_buf(uint8_t *buf, uint16_t buflen) {
  dac_state.buf = buf;
  dac_state.buflen = buflen;

#if 0
  for (uint16_t i = 0; i < buflen; i++) {
    logline(LEVEL_DEBUG_NOISY, "reg[%d] = 0x%02x", i, buf[i]);

  }
#endif
  return 0;
}

/**
 * \brief Start DAC output
 *
 * This enables the DAC output
 *
 * \param period The number of timer clocks per sample
 * \param oc_value The number of prescaled system ticks per timer half-clock
 *
 * Note that oc_value is *half* the period of the timer clock,
 * measured in prescaled system clock ticks.  Our default prescaler is
 * 1, so this is just half the number of clocks between sample advances.
 */
int dac_start(int period, uint32_t oc_value) {
  // Move pin to Analog mode
  gpio_mode_setup(DAC_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, DAC_PIN);

  //////////////////////////////
  // Set up timer

  rcc_periph_clock_enable(DAC_TIMER_CLOCK);
  // Reset its state
  rcc_periph_reset_pulse(DAC_TIMER_RESET);

  // Clock the timer with raw CPU clock, on the rising edge
  timer_set_mode(DAC_TIMER,
		 TIM_CR1_CKD_CK_INT,
		 TIM_CR1_CMS_EDGE,
		 TIM_CR1_DIR_UP);

  timer_continuous_mode(DAC_TIMER);

  timer_set_period(DAC_TIMER, period);

  // \todo Do we need to disable all other OCs here?
  timer_enable_oc_output(DAC_TIMER, DAC_TIMER_OUTPUT);

  timer_disable_oc_clear(DAC_TIMER, DAC_TIMER_OUTPUT);
  timer_disable_oc_preload(DAC_TIMER, DAC_TIMER_OUTPUT);

  timer_set_oc_slow_mode(DAC_TIMER, DAC_TIMER_OUTPUT);
  timer_set_oc_mode(DAC_TIMER, DAC_TIMER_OUTPUT, TIM_OCM_TOGGLE);
  timer_set_oc_value(DAC_TIMER, DAC_TIMER_OUTPUT, oc_value);
  timer_disable_preload(DAC_TIMER);

  timer_set_master_mode(DAC_TIMER, TIM_CR2_MMS_COMPARE_OC1REF);
  timer_enable_counter(DAC_TIMER);

  //////////////////////////////
  // Set up DMA
  rcc_periph_clock_enable(DAC_DMA_CLOCK);

  dma_stream_reset(DAC_DMA_CTRL, DAC_DMA_STREAM);
  dma_set_priority(DAC_DMA_CTRL, DAC_DMA_STREAM, DMA_SxCR_PL_LOW);

  // set ourselves to 8b outputs
  dma_set_memory_size(DAC_DMA_CTRL, DAC_DMA_STREAM, DMA_SxCR_MSIZE_8BIT);
  dma_set_peripheral_size(DAC_DMA_CTRL, DAC_DMA_STREAM, DMA_SxCR_PSIZE_8BIT);

  // Set up our memory accesses
  dma_enable_memory_increment_mode(DAC_DMA_CTRL, DAC_DMA_STREAM);
  dma_enable_circular_mode(DAC_DMA_CTRL, DAC_DMA_STREAM);
  // And set our transfer mode
  dma_set_transfer_mode(DAC_DMA_CTRL, DAC_DMA_STREAM,
			DMA_SxCR_DIR_MEM_TO_PERIPHERAL);

      // Set the peripheral destination address
  dma_set_peripheral_address(DAC_DMA_CTRL, DAC_DMA_STREAM,
			     (uint32_t) &DAC_DHR8R1(DAC_UNIT));

  // Set the source memory address and length
  dma_set_memory_address(DAC_DMA_CTRL, DAC_DMA_STREAM, (uint32_t) dac_state.buf);
  dma_set_number_of_data(DAC_DMA_CTRL, DAC_DMA_STREAM, dac_state.buflen);

  dma_channel_select(DAC_DMA_CTRL, DAC_DMA_STREAM, DAC_DMA_CHSEL);
  dma_enable_stream(DAC_DMA_CTRL, DAC_DMA_STREAM);


  //////////////////////////////
  // Set up DAC
  rcc_periph_clock_enable(RCC_DAC);
  dac_trigger_enable(DAC_UNIT, DAC_CHANNEL);
  dac_set_trigger_source(DAC_UNIT, DAC_TRIGGER_SEL);

  // And enable DAC
  dac_dma_enable(DAC_UNIT, DAC_CHANNEL);
  dac_enable(DAC_UNIT, DAC_CHANNEL);

  return 0;
}


void dma1_stream5_isr(void)
{
	if (dma_get_interrupt_flag(DMA1, DMA_STREAM5, DMA_TCIF)) {
		dma_clear_interrupt_flags(DMA1, DMA_STREAM5, DMA_TCIF);
		/* Toggle PC1 just to keep aware of activity and frequency. */
		gpio_toggle(GPIOC, GPIO1);
	}
}



/**
 * \brief Stop an ongoing DAC output
 *
 * Note that this leaves the DAC output pin in analog mode
 */
int dac_stop(void) {
  dac_disable(DAC_UNIT, DAC_CHANNEL);
  // Set pin back to INPUT for HiZ
  gpio_mode_setup(DAC_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE, DAC_PIN);

  // \todo actually stop DMA when stopping DAC output
  // \todo actually stop timer when stopping DAC output

  return 0;
}

/** \} */ // End doxygen group
