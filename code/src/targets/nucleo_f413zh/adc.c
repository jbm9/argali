/**
 * \file adc.c
 * \brief Nucleo F413ZH ADC interface implementation
 */


/**
 * \defgroup nucleo_f413zh_adc ADC driver (Nucleo F413ZH)
 *
 * \addtogroup nucleo_f413zh_adc
 * \{
 * \ingroup nucleo_f413zh
 *
 * This is a DMA-driven ADC input driver.
 *
 * Resources needed for the F413ZH's DMA ADC:
 *
 * * DMA channel
 * * ADC channel(s)
 * * Clock channels
 * * GPIO AF assignment
 *
 * Relevant documentation: RM0430 Rev 8 p341: 13.3.8 Scan mode
 * explains how continuous multi-channel scans work, and how they feed
 * into DMA.  This seems to be the way to get DMA working for ADC
 * inputs.
 *
 * * Set channels in the ADC_SQRx registers (including count in SQR1)
 * * Set the SCAN bit in ADC_CR1
 * * Data is stashed in ADC_DR
 * * If DMA is set, this gets moved to memory after the conversion
 *
 * p343/13.4 says data alignment is in ADC_CR2
 *
 * p344 13.5 is about setting channel sampling times.
 *
 * T_conv = Sampling time + 12 cycles
 *
 * The minimum sampling time is dependent on the resolution.  For N
 * bits, the minimum sampling time is N+12 clocks.
 *
 * These cycles are of ADCCLK, which is system clock after prescaler
 * specified in RM0430 13.3.2, but better specified in the register
 * descriptions.  p353 has ADC_CCR, where ADCPRE lives.
 *
 * Note also that there are TWO clocks for ADC: one for conversion,
 * and the other for register access.  The ADCCLK is derived from the
 * clock on APB2 by prescaling, but the actual register accesses
 * happen at the raw APB2 clock speed.
 *
 * There are some minimal limits to the ADC clock frequency, described
 * in the DS11581, p144.  We can't clock it faster than 15MHz
 *
 * ADC DMA setup is described in 13.8.1.  If you're after continuous
 * input (say you have a mic array), you'll want double-buffer
 * circular mode.
 *
 * Enable DMA (set the DMA bit in ADC_CR2).
 *
 * If, despite this, you get a DMA overrun (ADC_SR has OVR set), you
 * need to follow a procedure to reset state.  Your data is good, but
 * the process to reset is to clear the OVR flag and DMAEN in the DMA
 * stream, then re-init both DMA and ADC with all the conversion
 * channel info and data buffers.  This will re-sync your data stream
 * with the input conversions.
 *
 * Pin assignments are in DocID 029162 Rev6 pp50-64
 *
 * PA3 is ADC1_IN3, which is also CN9.A0 on the nucleo board.  This
 * lets us not open another GPIO port just yet.  I worry about
 * crosstalk from our boredom tone a bit, but we'll see how it goes.
 */

#include "adc.h"
#include "dma.h"
#include "leds.h"
#include "timer.h"

#include "logging.h"
#include "dtmf.h"

//////////////////////////////////////////////////////////////////////
// Debug Macros


//////////////////////////////////////////////////////////////////////
// State variables

static adc_dma_buffer_t dma_buffer; //!< The buffer that was given to us

//////////////////////////////////////////////////////////////////////
// Implementation code

/**
 * \brief Starts up the clocks needed for ADC
 */
static void adc_setup_clocks(void) {
  rcc_periph_clock_enable(RCC_GPIOA);
  rcc_periph_clock_enable(RCC_ADC1);
  rcc_periph_clock_enable(RCC_DMA2);
}

/**
 * \brief Configure the GPIOs needed for this
 *
 * This sets up our GPIOs for analog input.
 */
static void adc_setup_gpio(void) {
  // PA0 -- CN10.29
  gpio_mode_setup(GPIOA, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO0);

  // PA2 -- CN10.11
  gpio_mode_setup(GPIOA, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO2);
}


/**
 * \brief Configures the ADC peripheral for capture
 * */
static void adc_setup_adc(uint8_t *channels, uint8_t n_channels) {
  nvic_enable_irq(NVIC_ADC_IRQ);

  adc_power_off(ADC1); // Turn off ADC to configure sampling

  adc_set_resolution(ADC1, ADC_RESOLUTION_CR1);
  adc_set_sample_time_on_all_channels(ADC1, ADC_SMPR_SMP_112CYC);
  adc_set_dma_continue(ADC1);
  adc_set_regular_sequence(ADC1, n_channels, channels);
  adc_enable_dma(ADC1);
  adc_enable_overrun_interrupt(ADC1);

  adc_power_on(ADC1);  // Re-power ADC peripheral
}


/**
 * \brief Brings up the DMA with a configuration for our ADC.
 *
 * Note that this treats your buffer as a contiguous unit for two
 * half-length buffers.
 *
 * This does a full reset of the peripheral, then follows the flow
 * found in RM0430r8 p232, 9.3.18 "Stream configuration procedure"
 */
static void adc_setup_dma(uint8_t *buf, const uint32_t buflen) {
  // Before we begin, let's save this state for restarts
  dma_buffer.buf = buf;
  dma_buffer.buflen = buflen;

  dma_settings_t settings = {
                             .dma = DMA2,
                             .stream = DMA_STREAM0,
                             .channel = DMA_SxCR_CHSEL_0,
                             .priority = DMA_SxCR_PL_LOW,

                             .direction = DMA_SxCR_DIR_PERIPHERAL_TO_MEM,
                             .paddr = (uint32_t)&ADC_DR(ADC1),
                             .peripheral_size = DMA_SxCR_PSIZE_8BIT,
                             .buf = (uint32_t) buf,
                             .buflen = buflen,
                             .mem_size = DMA_SxCR_MSIZE_8BIT,

                             .circular_mode = 1,
                             .double_buffer = 1,

                             .transfer_complete_interrupt = 1,
                             .enable_irq = 1,
                             .irqn = NVIC_DMA2_STREAM0_IRQ,
  };

  dma_setup(&settings);
  return;
}


/**
 * \brief Pause the ADC
 *
 * This pauses the ADC, right now.  It does not necessarily align the
 * data, which is unfortunate.
 *
 * \todo This permahangs the ADC after two calls?
 *
 * \return The number of points remaining in the current buffer
 */
uint32_t adc_stop(void) {
  uint32_t pts_left;

  // Stop the ADC clock before stopping DMA, to avoid an endless loop
  // of overflows.
  rcc_periph_clock_disable(RCC_ADC1);

  // Interrupting DMA means we need a full rebuild afterwards.  See
  // RM0430r8 p230 9.3.15 DMA transfer suspension for details.

  // Note that this triggers an overflow ADC ISR, and will keep doing
  // so until the ADC clock is turned off!
  dma_disable_stream(DMA2, DMA_STREAM0);

  // After we stop the stream, find out where the data buffer wraps,
  // so we can report it back.
  pts_left = DMA_SNDTR(DMA2, DMA_STREAM0);

  return pts_left;
}

/**
 * \brief Unpause the ADC
 *
 * This turns the ADC data pipeline back on.  You must have called
 * adc_setup() prior to this.
 */
void adc_start(void) {
  rcc_periph_clock_enable(RCC_ADC1);
  adc_setup_dma(dma_buffer.buf, dma_buffer.buflen);
  adc_start_conversion_regular(ADC1);
}


/**
 * \brief Sets up ADC1 on DMA2 Stream 0 for input
 *
 * Note that desired_rate is a requested sampling rate, and the driver
 * may have to adjust the result.  You will therefore get back the
 * actual sampling rate.
 *
 *
 * \param prescaler The prescaler used for the timer clocking the ADC, see below
 * \param period The semi-period between timer clocks, see below
 * \param buff Buffer to write data into
 * \param buflen Number of entries in this buffer
 *
 * See timer_setup_adcdac() for more info on the prescaler and period
 * variables, as they are just passed through to it.
 *
 * \returns The actual sampling rate that was obtained
 */
float adc_setup(uint16_t prescaler, uint32_t period, uint8_t *buff, uint32_t buflen) {
  uint8_t channels[1] = {0};
  const uint8_t n_channels = 1;

  adc_setup_clocks();
  adc_setup_gpio();

  timer_setup_adcdac(TIM3, prescaler, period);

  adc_setup_adc(channels, n_channels);

  adc_enable_external_trigger_regular(ADC1,
                                      ADC_CR2_EXTSEL_TIM3_TRGO,
                                      ADC_CR2_EXTEN_RISING_EDGE);

  adc_setup_dma(buff, buflen);

  // And now we can kick off the ADC conversion
  adc_start_conversion_regular(ADC1);

  //  return sample_rate;

  return adc_get_sample_rate(prescaler, period);
}


float adc_get_sample_rate(uint16_t prescaler, uint32_t period) {
  uint32_t ck_in = rcc_get_timer_clk_freq(TIM3);
  return (ck_in/2)/(prescaler+1)/(period+1);
}


//////////////////////////////////////////////////////////////////////
// ISRs

/**
 * \brief DMA2 Stream0 ISR
 *
 * This is currently only used to clear the TCIF flag when transfers
 * are complete.  See RM0430r8 p235, the DMA_LISR register
 * documentation, and the rest of chapter 9 on all the ways this
 * interacts with the DMA peripheral.
 *
 * In particular, p215 says there are 5 flags that can trigger this
 * IRQ: 5 event flags (DMA half transfer, DMA transfer complete, DMA
 * transfer error, DMA FIFO error, direct mode error) logically ORed
 * together in a single interrupt request for each stream

 */
void dma2_stream0_isr(void) {
  if((DMA2_LISR & DMA_LISR_TCIF0) != 0) {
    // Clear this flag so we can continue
    dma_clear_interrupt_flags(DMA2, DMA_STREAM0, DMA_LISR_TCIF0);

    uint8_t *bufpos = dma_buffer.buf;
    if (dma_get_target(DMA2, DMA_STREAM0)) {
      bufpos += dma_buffer.buflen/2;
    }

    dtmf_process(bufpos, dma_buffer.buflen/2);
  }
}

/**
 * \brief ADC ISR
 *
 * This is used to clear the overrun flag if it's hit.  It will also
 * be called in other situations (RM0430r8 p350, 13.11 "ADC
 * Interrupts"), but all we care about is recovering from overrun
 * situations.
 *
 * In our case, this happens every time we turn off DMA for the ADC
 * input.  That generates an interrupt, and we need to clear the
 * overrun flag before it's possible to use the ADC with DMA again
 * (p347, 13.8.1 "Using the DMA").
 *
 * p347 also describes how to recover from this state, which boils
 * down to "reinitialize the DMA from scratch."
 */
void adc_isr(void) {
  if (adc_get_overrun_flag(ADC1)) {
    adc_clear_overrun_flag(ADC1);
  }
}
