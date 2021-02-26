#pragma once

/**
 * \file system_clock.h
 * \brief Header file for system clock initialization routine(s)
 */

#define HSE_CLOCK_MHZ 8 //!< An 8MHz clock from ST-Link MCO (default source on MB1137 Nucleo-144 boards per UM1974r8)

#define CPU_CLOCK_SPEED 84000000 //!< Main clock speed

#define AHB_TICKS_PER_DELAY_LOOP 7 //!< How many AHB clock ticks our _delay_ms() takes for a single loop

void system_clock_setup(void);
void _delay_ms(uint16_t);
