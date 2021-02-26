#pragma once

/**
 * \file system_clock.h
 * \brief Header file for system clock initialization routine(s) (Nucleo F767ZI)
 */


#define HSE_CLOCK_MHZ 8 //!< An 8MHz clock from ST-Link MCO (default source on MB1137 Nucleo-144 boards per UM1974r8)


#define CPU_CLOCK_SPEED 216000000 //!< Main clock speed

#define CLOCKS_PER_MS CPU_CLOCK_SPEED / 1000 //!< Approximate number of NOP/inc/cmp loop cycles to run to delay a millisecond


void system_clock_setup(void);
void _delay_ms(uint16_t);
