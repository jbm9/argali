/**
 * \file dac.c
 * \brief DAC driver (Nucleo F767ZI)
 */

#include "dac.h"
#include "dma.h"
#include "timer.h"

#include "logging.h"

/**
 * \defgroup nucleo_f767zi_dac DAC Driver (Nucleo F767ZI)
 * \{
 */

//////////////////////////////////////////////////////////////////////
// Debug Macros


//////////////////////////////////////////////////////////////////////
// State variables

static uint16_t last_prescaler;
static uint32_t last_period;
static const uint8_t *last_waveform;
static uint16_t last_npoints;

//////////////////////////////////////////////////////////////////////
// Implementation code


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
static void dac_dma_setup(const uint8_t *waveform, uint16_t npoints)
{
  dma_settings_t settings = {
                             .dma = DMA1,
                             .stream = DMA_STREAM5,
                             .channel = DMA_SxCR_CHSEL_7,
                             .priority = DMA_SxCR_PL_LOW,

                             .direction = DMA_SxCR_DIR_MEM_TO_PERIPHERAL,
                             .paddr = (uint32_t) &DAC_DHR8R1(DAC1),
                             .peripheral_size = DMA_SxCR_PSIZE_8BIT,
                             .buf = (uint32_t) waveform,
                             .buflen = npoints,
                             .mem_size = DMA_SxCR_MSIZE_8BIT,

                             .circular_mode = 1,
                             .double_buffer = 0,

                             .transfer_complete_interrupt = 0,
                             .enable_irq = 1,
                             .irqn = NVIC_DMA1_STREAM5_IRQ,
  };

  dma_setup(&settings);
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
 * There is a relevant erratum in the F767 here: 2.6.1 in ES0334 shows
 * that stopped DMA can lurk in the system and pop out at the next
 * re-enable, before the new wavetable, and needs to be worked around.
 *
 */
void dac_setup(uint16_t prescaler, uint32_t period, const uint8_t *waveform, uint16_t npoints) {
  // Stash these in case we need to do the erratum workaround
  last_prescaler = prescaler;
  last_period = period;
  last_waveform = waveform;
  last_npoints = npoints;

  gpio_setup();
  timer_setup_adcdac(TIM2, prescaler, period);

  rcc_periph_clock_enable(RCC_DMA1);
  dac_dma_setup(waveform, npoints);

  /* Enable the DAC clock on APB1 */
  rcc_periph_clock_enable(RCC_DAC);
  /* Setup the DAC channel 1, with timer 2 as trigger source.
   * Assume the DAC has woken up by the time the first transfer occurs */
  dac_trigger_enable(DAC1, DAC_CHANNEL1);
  dac_set_trigger_source(DAC1, DAC_CR_TSEL1_T2);  // RM0410r4 p490
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
  // We may need to Work around 2.6.2 "DMA request not automatically
  // cleared by clearing DMAEN" from ES0334r7.  It doesn't seem like
  // we are seeing this in practice, though, so we will only log it
  // for now.

  // 1. Check if DMAUDR bit is set in DAC_CR.
  if (DAC_SR(DAC1) & DAC_SR_DMAUDR1) {
    logline(LEVEL_ERROR, "DMAUDR bit set at dac_start, may get noise out before tone. SR=%08x", DAC_SR(DAC1));
    logline(LEVEL_DEBUG_NOISY, "DMA1 LISR=%08x HISR=%08x", DMA_LISR(DMA1), DMA_HISR(DMA1));

#ifdef F767_ATTEMPT_DAC_DMA_WORKAROUND_2_6_2
    // 2. Clear the DAC channel DMAEN bit.
    dac_disable(DAC1, DAC_CHANNEL1);
    // 3. Disable the DAC clock.
    rcc_periph_clock_disable(RCC_DAC);
    // 4. Reconfigure the DAC, DMA and the triggers.
    dac_setup(last_prescaler, last_period, last_waveform, last_npoints);

    // 5. Restart the application.
    // fallthrough to the rest of this routine
#endif
  }
  dac_dma_enable(DAC1, DAC_CHANNEL1);
  dma_enable_stream(DMA1, DMA_STREAM5);
  dac_enable(DAC1, DAC_CHANNEL1);
}

/**
 * Stop the DAC output
 *
 * This stops the DAC output.
 */
void dac_stop(void) {
  dac_dma_disable(DAC1, DAC_CHANNEL1);
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



//////////////////////////////////////////////////////////////////////
// ISRs

/**
 * \brief DMA Callback ISR for DAC (currently empty)
 */
void dma1_stream5_isr(void)
{
}

/** \} */
