#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>

#include "system_clock.h"

/**
 * \file system_clock.c
 * \brief System clock driver (Nucleo F767ZI)
 */

/**
 * \defgroup nucleo_f767zi Drivers: Nucleo F767ZI
 * \{
 * Drivers for the Nucleo F767ZI evaluation board
 * \}
 */

/**
 * \defgroup nucleo_f767zi_system_clock System Clock driver (Nucleo F767ZI)
 * \{
 * \ingroup nucleo_f767zi
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
 * code.  If used in anything like production code, should probably
 * have a verification and validation plan.
 */
void _delay_ms(uint16_t ms) {
  const uint32_t loops_per_ms = rcc_ahb_frequency / 1000 / AHB_TICKS_PER_DELAY_LOOP;

  for (uint32_t i = 0; i < ms * loops_per_ms; i += 1)
    __asm__("NOP");
}

/** \} */  // End doxygen group
