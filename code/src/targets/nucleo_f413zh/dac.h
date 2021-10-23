#pragma once

/**
 * \file dac.h
 * \brief Header for DAC driver (Nucleo F413ZH)
 */

/**
 * \addtogroup nucleo_f413zh_dac
 * \{
 */

#ifdef TEST_UNITY
#warning Building without hardware DAC support
#else

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/dac.h>
#include <libopencm3/stm32/dma.h>

#endif

void dac_setup(uint16_t, uint32_t, const uint8_t*, uint16_t);
float dac_get_sample_rate(uint16_t, uint32_t);

void dac_start(void);
void dac_stop(void);

/** \} */
