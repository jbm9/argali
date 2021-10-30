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
 * input (say you have a mic array), you'll want doubl-buffer circular
 * mode.
 *
 * Enable DMA (set the DMA bit in ADC_CR2).  Note that we probably
 * also need to set the DDS bit to enable continuous circular DMA.
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

 *
 * DMA Details: Chapter 9 of RM0430, p214
 *
 * DMA Channel/Stream request mapping table is p218.
 * ADC1 is DMA2 Channel 0 stream 1 or 4
 *
 * Circular mode is p223: 9.3.9: set the CIRC bit in DMA_SxCR, and
 * need to make sure the MBURST is one of 4/8/16.  Also need the
 * memory and peripheral record sizes to be simple power of two or
 * four {1/4,1/2,1,2,4}.

 * Double-buffer mode is p223: 9.3.10.  DBM set in DMA_SxCR register.
 * This automatically turns on circular for you (ignoring CIRC).  At
 * the end of each transaction, the buffer pointers are swapped.
 * Relevant App Note is AN4031.
 * AN4031 pp14&5 1.2 is a schematization of setting up a DMA transfer.
 *
 * Note that any time we interrupt the DMA peripheral, we need to
 * reconfigure it fully before restarting.
 */

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/dma.h>
#include "adc.h"

#include "leds.h"
#include "logging.h"

#include "dtmf.h"

//////////////////////////////////////////////////////////////////////
// Debug Macros

#ifdef ADC_INCLUDE_REGISTER_DUMPING

#define ADCJANKXSTR(x) ADCJANKSTR(x)  //!< Stage 2 of a janky two-stage macro-stringifier
#define ADCJANKSTR(x) #x       //!< Stage 1 of a janky two-stage macro-stringifier

static void adc_dump_registers(void) {
#define R(s) logline(LEVEL_DEBUG_NOISY, ADCJANKSTR(s) ":\t%08x", s(ADC1));
  R(ADC_SR);
  R(ADC_CR1);
  R(ADC_CR2);
  // Skipping injected
  // Skipping watchdogs
  R(ADC_SMPR1);
  R(ADC_SMPR2);
  R(ADC_SQR1);
  R(ADC_SQR2);
  R(ADC_SQR3);
  // Skipping injected
  // R(ADC_DR); // This manipulates state, eg ADC_SR's EOC p351
  //  R(ADC_CSR); // Pointless on F413, pp362&3
  // R(ADC_CCR);  // Also pointles on F413, p363
#undef R
}

static void dma_dump_registers(void) {
#define R(s) logline(LEVEL_DEBUG_NOISY, ADCJANKSTR(s) ":\t%08x", s(DMA2));
#define RS(s) logline(LEVEL_DEBUG_NOISY, ADCJANKSTR(s) ":\t%08x", s(DMA2,0));
  R(DMA_LISR);
  R(DMA_HISR);
  R(DMA_LIFCR);
  R(DMA_HIFCR);
  RS(DMA_SCR);
  RS(DMA_SNDTR);
  RS(DMA_SPAR);
  RS(DMA_SM0AR);
  RS(DMA_SM1AR);
  RS(DMA_SFCR);
#undef R
#undef RP
}

#endif

//////////////////////////////////////////////////////////////////////
// State variables

static adc_dma_buffer_t dma_buffer; //!< The buffer that was given to us
static adc_freq_config_t freq_config; //!< Currently-applied frequency config

// From libopencm3's adc.h:
// A coindexed array to the ADC_SMPR_SMP_xCYC values, with sentinel
static const uint16_t possible_times[] = { 3, 15, 28, 56, 84, 112, 144, 480, 0};

// A coindexed array to the ADC_CCR_ADCPRE_BYx values
static const uint8_t possible_prescales[] = { 2, 4, 6, 8, 0};


//////////////////////////////////////////////////////////////////////
// Implementation code

/**
 * \brief Starts up the clocks needed for ADC
 */
static void adc_setup_clocks(void) {
  rcc_periph_clock_enable(RCC_GPIOA);
  rcc_periph_clock_enable(RCC_ADC1);
  rcc_periph_clock_enable(RCC_DMA2);

  // Timer3, RM0430r8 p534 intro
  // Enable TIM3 clock.
  rcc_periph_clock_enable(RCC_TIM3);
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
 * \brief Set up the timer for the ADC subsystem
 *
 * \param prescaler Sets the prescaler to this (plus 1)
 * \param period How many timer clocks before an OC clock (plus 1)
 *
 * So the expected sampling rate is
 * TIM2CLK/(prescaler+1)/(period+1)/2, since we toggle the OC line
 * every time we hit it, but only clock the DAC on rising edges.
 *
 * TIMxCLK is derived from different sources, described in RM0430r8
 * p121, section 6.2 Clocks
 *
 * If RCC_DCKCFGR has TIMPRE cleared, and APB prescaler is 1, then
 * TIMxCLK HCLK.
 *
 * If RCC_DKCFGR1 has TIMPRE cleared, and APB prescaler is not 1, then
 * TIMxCLK is 2*xPCLKx
 *
 * If RCC_DKCFGR1 has TIMPRE set, and APB prescaler is 1 or 2, then
 * TIMxCLK is HCLK
 *
 * If RCC_DKCFGR1 has TIMPRE cleared, and APB prescaler is not 1 or 2,
 * then TIMxCLK is 4*PCLKx
 *
 * This is a helpful breakdown of the clocking, though it's a bit
 * gnarly.

 *
 * RCC_DCKCFGR1 is on pp177&8, 6.3.27.
 *
 * Note that libopencm3 provides rcc_get_timer_clk_freq(), which
 * manages all of the computation to figure out what your timer is
 * clocked to, including the system clock.  The system clock speed is
 * nicely managed by rcc_clock_setup_hse(), so we should be okay to
 * use that.
 *
 * Also note that libopencm3 offers no API into the relevant bits to
 * change those values, so we're stuck with the default state unless
 * we write our own implementation.
 */
static void timer_setup(uint16_t prescaler, uint32_t period)
{

  // Reset the timer
  rcc_periph_reset_pulse(RST_TIM3);

  // Timer 2 mode: - 4x oversample, Alignment edge, Direction up
  // This is all in TIM3_CR1, p572 18.4.1
  timer_set_mode(TIM3, TIM_CR1_CKD_CK_INT_MUL_4,
		 TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);

  // Set the prescaler TIM3_PSC, p587 18.4.11
  timer_set_prescaler(TIM3, prescaler);

  // Also TIM3_CR1
  timer_continuous_mode(TIM3);

  // Set the period between ticks, TIM3_ARR, p587, 18.4.12
  timer_set_period(TIM3, period);

  // Disable OC outputs we don't use, TIM_CCER pp585&6 18.4.9
  timer_disable_oc_output(TIM3, TIM_OC2);
  timer_disable_oc_output(TIM3, TIM_OC3);
  timer_disable_oc_output(TIM3, TIM_OC4);

  // And enable our output on OC1, also TIM_CCER
  timer_enable_oc_output(TIM3, TIM_OC1);

  // Lots of OC mangling, all in TIM3_CCMR1, pp581-3 18.4.7
  timer_disable_oc_clear(TIM3, TIM_OC1);
  timer_disable_oc_preload(TIM3, TIM_OC1);
  timer_set_oc_slow_mode(TIM3, TIM_OC1);
  timer_set_oc_mode(TIM3, TIM_OC1, TIM_OCM_TOGGLE);
  // Ends TIM3_CCMR1 mangling

  // Set the timer trigger output (for the DAC) to the channel 1
  // output compare.  TIM3_CR2, p574 18.4.2

  // This controls when TRGO is sent to downstream clocks, also TIM_CR2
  timer_set_master_mode(TIM3, TIM_CR2_MMS_COMPARE_OC1REF);

  // And start the timer up
  timer_enable_counter(TIM3);
}


/**
 * \brief Configures the ADC peripheral for capture
 * */
static void adc_setup_adc(uint8_t *channels, uint8_t n_channels) {
  nvic_enable_irq(NVIC_ADC_IRQ);

  adc_power_off(ADC1); // Turn off ADC to configure sampling

  adc_set_resolution(ADC1, ADC_RESOLUTION_CR1);
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

  // 1. Set to disabled
  dma_stream_reset(DMA2, DMA_STREAM0); // Sets EN = 0, resets DMA

  // 2. Set the port register address
  dma_set_peripheral_address(DMA2, DMA_STREAM0,
			     (uint32_t)&ADC_DR(ADC1));

  // 3. Set the memory address in DMA_SxMA0R (and SxMA1R)
  dma_set_memory_address(DMA2, DMA_STREAM0, (uint32_t) buf);
  dma_set_memory_address_1(DMA2, DMA_STREAM0, (uint32_t) (buf+buflen/2));

  // 4. Configure the data items to transfer
  dma_set_number_of_data(DMA2, DMA_STREAM0, buflen);

  // 5. Select the channel
  dma_channel_select(DMA2, DMA_STREAM0, DMA_SxCR_CHSEL_0);

  // 6. If the peripheral is a flow controller, set that up (this isn't)

  // 7. Configure priority
  dma_set_priority(DMA2, DMA_STREAM0, DMA_SxCR_PL_LOW);

  // 8. Configure FIFO (not using one)
  dma_enable_direct_mode(DMA2, DMA_STREAM0);  // Don't use the FIFO

  // 9. Configure the transfer direction etc (see document)
  dma_set_transfer_mode(DMA2, DMA_STREAM0, DMA_SxCR_DIR_PERIPHERAL_TO_MEM);

  dma_enable_memory_increment_mode(DMA2, DMA_STREAM0);
  dma_set_peripheral_size(DMA2, DMA_STREAM0, DMA_SxCR_PSIZE_8BIT);
  dma_set_memory_size(DMA2, DMA_STREAM0, DMA_SxCR_MSIZE_8BIT);

  dma_enable_circular_mode(DMA2, DMA_STREAM0);

  dma_enable_double_buffer_mode(DMA2, DMA_STREAM0);
  dma_enable_transfer_complete_interrupt(DMA2, DMA_STREAM0);

  nvic_enable_irq(NVIC_DMA2_STREAM0_IRQ);
  // 10. Activate the stream
  dma_enable_stream(DMA2, DMA_STREAM0);
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
uint32_t adc_pause(void) {
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
 * This turns the ADC data pipeline back on
 */
void adc_unpause(void) {
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
 * \param buff Buffer to write data into
 * \param buflen Number of entries in this buffer
 *
 * \returns The actual sampling rate that was obtained
 */
float adc_setup(uint8_t *buff, const uint32_t buflen) {
  uint16_t prescaler = 104;
  uint32_t period = 49;

  uint8_t channels[1] = {0};
  const uint8_t n_channels = 1;

  adc_setup_clocks();

  adc_setup_gpio();
  timer_setup(prescaler, period);

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
  }

  uint8_t *bufpos = dma_buffer.buf;
  if (dma_get_target(DMA2, DMA_STREAM0)) {
    bufpos += dma_buffer.buflen/2;
  }

  led_green_toggle();

  dtmf_process(bufpos, dma_buffer.buflen/2);
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
