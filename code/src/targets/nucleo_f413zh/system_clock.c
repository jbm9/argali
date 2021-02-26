#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>

#include "system_clock.h"

/**
 * \file system_clock.c
 * \brief System clock driver for the Nucleo F413ZH evaluation board
 */

/**
 * \brief Set up the system clock at startup
 */
void system_clock_setup(void) {
  rcc_clock_setup_pll(&rcc_hse_8mhz_3v3[RCC_CLOCK_3V3_84MHZ]);
  // noop
}


/**
 * \brief A janky, approximate, busy-loop delay function
 *
 * \param ms The approximate time to sleep for (in milliseconds)
 *
 * Note that this is wildly inadequate for anything but silly sample
 * code.  If used in anything like production code, should probably
 * have a verification and validation plan.
 */
void _delay_ms(uint16_t ms) {
  const uint32_t loops_per_ms = rcc_ahb_frequency / 1000 / AHB_TICKS_PER_DELAY_LOOP;

  for (uint32_t i = 0; i < ms * loops_per_ms; i += 1)
    __asm__("NOP");
}
