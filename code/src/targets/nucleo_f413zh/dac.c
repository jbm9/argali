/**
 * \file dac.c
 * \brief DAC driver (Nucleo F413ZH)
 */

#include "dac.h"

/**
 * \defgroup nucleo_f413zh_dac DAC Driver (Nucleo F413ZH)
 * \{
 */


/**
 * \brief Set up the GPIOs for the DAC subsystem
 */
static void gpio_setup(void) {
  rcc_periph_clock_enable(RCC_GPIOA);
  // Set PA4 for DAC channel 1 to analogue, ignoring drive mode.
  // CN7.17
  gpio_mode_setup(GPIOA, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO4);
}

/**
 * \brief Set up the timer for the DAC subsystem
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
  // Timer2, RM0430r8 p534 intro
  // Enable TIM2 clock.
  rcc_periph_clock_enable(RCC_TIM2);

  // Reset the timer
  rcc_periph_reset_pulse(RST_TIM2);

  // Timer 2 mode: - 4x oversample, Alignment edge, Direction up
  // This is all in TIM2_CR1, p572 18.4.1
  timer_set_mode(TIM2, TIM_CR1_CKD_CK_INT_MUL_4,
		 TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);

  // Set the prescaler TIM2_PSC, p587 18.4.11
  timer_set_prescaler(TIM2, prescaler);

  // Also TIM2_CR1
  timer_continuous_mode(TIM2);

  // Set the period between ticks, TIM2_ARR, p587, 18.4.12
  timer_set_period(TIM2, period);

  // Disable OC outputs we don't use, TIM_CCER pp585&6 18.4.9
  timer_disable_oc_output(TIM2, TIM_OC2);
  timer_disable_oc_output(TIM2, TIM_OC3);
  timer_disable_oc_output(TIM2, TIM_OC4);

  // And enable our output on OC1, also TIM_CCER
  timer_enable_oc_output(TIM2, TIM_OC1);

  // Lots of OC mangling, all in TIM2_CCMR1, pp581-3 18.4.7
  timer_disable_oc_clear(TIM2, TIM_OC1);
  timer_disable_oc_preload(TIM2, TIM_OC1);
  timer_set_oc_slow_mode(TIM2, TIM_OC1);
  timer_set_oc_mode(TIM2, TIM_OC1, TIM_OCM_TOGGLE);
  // Ends TIM2_CCMR1 mangling

  // Set the timer trigger output (for the DAC) to the channel 1
  // output compare.  TIM2_CR2, p574 18.4.2

  // This controls when TRGO is sent to downstream clocks, also TIM_CR2
  timer_set_master_mode(TIM2, TIM_CR2_MMS_COMPARE_OC1REF);

  // And start the timer up
  timer_enable_counter(TIM2);
}


/**
 * \brief Initialize the DMA channel for DAC output
 *
 * This brings up our DMA channel and registers the waveform to
 * output.  It begins the DMA in circular mode, so it just goes and
 * goes after this call.
 *
 * See dac_setup() for how everything hangs together, and specifically
 * how this all gets clocked.
 *
 * \param waveform The points to output on the DAC
 * \param npoints How many points are in the waveform
 */
static void dma_setup(const uint8_t *waveform, uint16_t npoints)
{
  /* DAC channel 1 uses DMA controller 1 Stream 5 Channel 7. */
  /* Enable DMA1 clock and IRQ */
  rcc_periph_clock_enable(RCC_DMA1);
  nvic_enable_irq(NVIC_DMA1_STREAM5_IRQ);
  dma_stream_reset(DMA1, DMA_STREAM5);
  dma_set_priority(DMA1, DMA_STREAM5, DMA_SxCR_PL_LOW);
  dma_set_memory_size(DMA1, DMA_STREAM5, DMA_SxCR_MSIZE_8BIT);
  dma_set_peripheral_size(DMA1, DMA_STREAM5, DMA_SxCR_PSIZE_8BIT);
  dma_enable_memory_increment_mode(DMA1, DMA_STREAM5);
  dma_enable_circular_mode(DMA1, DMA_STREAM5);
  dma_set_transfer_mode(DMA1, DMA_STREAM5,
			DMA_SxCR_DIR_MEM_TO_PERIPHERAL);
  /* The register to target is the DAC1 8-bit right justified data
     register */
  dma_set_peripheral_address(DMA1, DMA_STREAM5, (uint32_t) &DAC_DHR8R1(DAC1));

  /* The array v[] is filled with the waveform data to be output */
  dma_set_memory_address(DMA1, DMA_STREAM5, (uint32_t) waveform);
  dma_set_number_of_data(DMA1, DMA_STREAM5, npoints);
  dma_disable_transfer_complete_interrupt(DMA1, DMA_STREAM5);
  dma_channel_select(DMA1, DMA_STREAM5, DMA_SxCR_CHSEL_7);
}

/**
 * \brief Set up a DAC channel for continuous output
 *
 * \param prescaler The prescaler used for the timer clocking the DAC, see below
 * \param period The semi-period between timer clocks, see below
 * \param waveform A buffer containing uint8_t data to send out the DAC
 * \param npoints The number of samples before looping
 *
 * At a high level: this sets up the DAC to DMA a buffer in a loop,
 * driven by a Timer peripheral.  Internally, we set up the timer,
 * then configure DMA, and finally connect it to the DAC.
 *
 * To begin the DAC output, you will need to call dac_start().
 *
 * In more detail:
 *
 * The waveform and npoints are straightforward: you populate an array
 * of data that's npoints long, and pass in a pointer to it.  If you
 * subsequently update that buffer, the output data is updated.
 *
 * Now, to the other two parameters: the prescaler and the period.
 * There is a complicated state of affairs here, which all starts with
 * the clock into the Timer used.  This depends on how you configured
 * your system clock, but you can find what it is from
 * rcc_get_timer_clk_freq().  Then, we divide that by `(prescaler+1)`
 * (making a prescaler input of `1` halving the frequency versus an
 * input of `0`).  This prescaled clock is then used to increment an
 * internal counter up to `period+1`.  When the timer reaches that
 * point, it toggles one of its chip-internal output lines.  The DAC
 * is then configured to use that line as its input clock, on the
 * rising edge.  So, every two toggles is a single clock into the DAC.
 *
 * The net result here is that the sampling frequency is
 *
 *   rcc_get_timer_clk_freq()/(prescaler+1)/(period+1)/2
 *
 * But you don't actually need to compute that yourself: we provide
 * dac_get_sample_rate() to accomplish this all for you.
 *
 * Another note: prescaler may be zero (no prescaling is done), but
 * period must be at least 1.  Thus, the fastest this setup can clock
 * the DAC data out is rcc_get_timer_clk_freq()/4
 *
 * NB: while it is slightly unclear in the reference manual, we have
 * confirmed that both prescaler and period use the `x+1` form by
 * setting them to very low values and examining the resultant
 * waveforms.
 *
 */
void dac_setup(uint16_t prescaler, uint32_t period, const uint8_t *waveform, uint16_t npoints) {
  gpio_setup();
  timer_setup(prescaler, period);
  dma_setup(waveform, npoints);

  /* Enable the DAC clock on APB1 */
  rcc_periph_clock_enable(RCC_DAC);
  /* Setup the DAC channel 1, with timer 2 as trigger source.
   * Assume the DAC has woken up by the time the first transfer occurs */
  dac_trigger_enable(DAC1, DAC_CHANNEL1);
  dac_set_trigger_source(DAC1, DAC_CR_TSEL1_T2);
  dac_dma_enable(DAC1, DAC_CHANNEL1);
}


/**
 * Starts the DAC output up
 *
 * This starts/restarts the DAC to output the current waveform buffer.
 * It does not do much to guarantee the present state/configuration of
 * the DAC, though.  You will need to do dac_setup() first.
 */
void dac_start(void) {
  dma_enable_stream(DMA1, DMA_STREAM5);
  dac_enable(DAC1, DAC_CHANNEL1);
}

/**
 * Stop the DAC output
 *
 * This stops the DAC output.
 */
void dac_stop(void) {
  dac_disable(DAC1, DAC_CHANNEL1);
  dma_disable_stream(DMA1, DMA_STREAM5);
}

/**
 * \brief Compute the sample rate given the prescaler/period settings
 *
 * \param prescaler The prescaler to use in dac_setup
 * \param period The period to use in dac_setup
 *
 * This uses the current clock configuration to compute the value.  If
 * you change the system clock, the value returned here will be
 * invalid]
 */
float dac_get_sample_rate(uint16_t prescaler, uint32_t period) {
  uint32_t ck_in = rcc_get_timer_clk_freq(TIM2);
  return (ck_in/2)/(prescaler+1)/(period+1);
}

/**
 * \brief DMA Callback ISR
 */
void dma1_stream5_isr(void)
{
}

/** \} */
