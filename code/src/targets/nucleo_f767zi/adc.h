#pragma once
/**
 * \file adc.h
 * \brief Header for ADC driver (Nucleo F767ZI)
 */

/**
 * \addtogroup nucleo_f767zi_adc
 * \{
 */

#ifdef TEST_UNITY
#warning Building without Hardware ADC support
#else

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/dma.h>

/**
 * \brief A trivial encapsulation of a memory buffer for DMA access.
 *
 */
typedef struct adc_dma_buffer {
  uint8_t *buf;  //!< A pointer to the buffer to fill
  uint16_t buflen; //!< The length of the buffer
} adc_dma_buffer_t;


#define ADC_PRESCALER_8KHZ 134 //!< The prescaler needed to get 8kHz
#define ADC_PERIOD_8KHZ 49 //!< The period needed to get 8kHz

float adc_setup(uint16_t, uint32_t, uint8_t *,uint32_t);
uint32_t adc_stop(void);
void adc_start(void);

float adc_get_sample_rate(uint16_t, uint32_t);
#endif

/** \} */
