#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>

#include "system_clock.h"

/**
 * \file system_clock.c
 * \brief System clock driver for the Nucleo F767ZI evaluation board
 */

/**
 * \brief Set up the system clock at startup
 */
void system_clock_setup(void) {
  // Turn on the HSE for the above speed
  rcc_clock_setup_hse(&rcc_3v3[RCC_CLOCK_3V3_216MHZ], HSE_CLOCK_MHZ);
  
}


/**
 * \brief A janky, approximate, busy-loop delay function
 *
 * \param ms The approximate time to sleep for (in milliseconds)
 *
 * Note that this is wildly inadequate for anything but silly sample
 * code.  It should be using a known-good configuration of the system
 * clock, and, if used in anything like production code, should
 * probably have a verification and validation plan.
 */
void _delay_ms(uint16_t ms) {
  //! \todo actually use the clock speed for delays
  for (uint32_t i = 0; i < ms * CLOCKS_PER_MS; i++)
    __asm__("NOP");
}
