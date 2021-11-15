#pragma once

/**
 * \file timer.h
 * \brief Header for Timer driver (Nucleo F767ZI)
 */

/**
 * \addtogroup nucleo_f767zi_timer
 * \{
 */

#ifdef TEST_UNITY
#warning Building without Hardware Timer support
#else

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/dma.h>

void timer_setup_adcdac(uint32_t, uint16_t, uint32_t);

#endif
/** \} */
