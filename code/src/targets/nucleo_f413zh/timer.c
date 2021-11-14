/**
 * \file timer.c
 * \brief Nucleo F413ZH timer interface implementation
 */


/**
 * \defgroup nucleo_f413zh_timer Timer driver (Nucleo F413ZH)
 *
 * \addtogroup nucleo_f413zh_timer
 * \{
 * \ingroup nucleo_f413zh
 *
 * This is a thin layer to abstract out the Timer setup for the ADC
 * and DAC peripheral drivers.
 */

#include "adc.h"
#include "timer.h"

/**
 * Set up a timer peripheral for our ADC and DAC drivers
 *
 * \param timer_peripheral A TIMx value from libopencm3
 * \param prescaler The raw prescaler value for the peripheral
 * \param period The raw period value for the peripheral
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
void timer_setup_adcdac(uint32_t timer_peripheral, uint16_t prescaler, uint32_t period) {

  // \todo Do we actually need to enable these clocks to run configuration?
  if (TIM3 == timer_peripheral) { // TIM3 used for ADC
    // Timer3, RM0430r8 p534 intro
    // Enable TIM3 clock.
    rcc_periph_clock_enable(RCC_TIM3);
    // Reset the timer
    rcc_periph_reset_pulse(RST_TIM3);
  } else if (TIM2 == timer_peripheral) { // TIM2 used for DAC
    // Timer2, also RM0430r8 p534 intro
    // Enable TIM2 clock.
    rcc_periph_clock_enable(RCC_TIM2);
    // Reset the timer
    rcc_periph_reset_pulse(RST_TIM2);
  }

  // Timer mode: - 4x oversample, Alignment edge, Direction up
  // This is all in TIM3_CR1, p572 18.4.1
  timer_set_mode(timer_peripheral, TIM_CR1_CKD_CK_INT_MUL_4,
		 TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);

  // Set the prescaler TIMx_PSC, p587 18.4.11
  timer_set_prescaler(timer_peripheral, prescaler);

  // Also TIMx_CR1
  timer_continuous_mode(timer_peripheral);

  // Set the period between ticks, TIMx_ARR, p587, 18.4.12
  timer_set_period(timer_peripheral, period);

  // Disable OC outputs we don't use, TIM_CCER pp585&6 18.4.9
  timer_disable_oc_output(timer_peripheral, TIM_OC2);
  timer_disable_oc_output(timer_peripheral, TIM_OC3);
  timer_disable_oc_output(timer_peripheral, TIM_OC4);

  // And enable our output on OC1, also TIM_CCER
  timer_enable_oc_output(timer_peripheral, TIM_OC1);

  // Lots of OC mangling, all in TIMx_CCMR1, pp581-3 18.4.7
  timer_disable_oc_clear(timer_peripheral, TIM_OC1);
  timer_disable_oc_preload(timer_peripheral, TIM_OC1);
  timer_set_oc_slow_mode(timer_peripheral, TIM_OC1);
  timer_set_oc_mode(timer_peripheral, TIM_OC1, TIM_OCM_TOGGLE);
  // Ends TIMx_CCMR1 mangling

  // Set the timer trigger output (for the DAC) to the channel 1
  // output compare.  TIMx_CR2, p574 18.4.2

  // This controls when TRGO is sent to downstream clocks, also TIMx_CR2
  timer_set_master_mode(timer_peripheral, TIM_CR2_MMS_COMPARE_OC1REF);

  // And start the timer up
  timer_enable_counter(timer_peripheral);
}

/** \} */
