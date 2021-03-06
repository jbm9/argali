#pragma once



/**
 * \file adc.h
 * \brief Header for ADC driver (Nucleo F413ZH)
 */

/**
 * \addtogroup nucleo_f413zh_adc
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
 * Absolute max sample rate per DS11581 p82
 *
 * The datasheet calls out the 2.4Msps as the absolute upper limit
 * when Vdda is 2.4-3.6V, and only 1.2Msps for 1.7-2.4V
 */
#define ADC_MAX_SAMPLE_RATE 2400000

/**
 * \brief A trivia encapsulation of a memory buffer for DMA access.
 *
 */
typedef struct adc_dma_buffer {
  uint8_t *buf;  //!< A pointer to the buffer to fill
  uint16_t buflen; //!< The length of the buffer
} adc_dma_buffer_t;

/**
 * This bundles up the ADC clock settings so we can populate them
 * automatically.
 *
 * Note that these appear to be random integers, but map to prescaler
 * and sample time settings per the datasheet (or libopencm3 macros).
 */
typedef struct adc_freq_config {
  uint32_t prescaler; //!< The prescaler to use, one of ADC_CCR_ADCPRE_BY2 etc
  uint8_t sample_time; //!< The sample time to use, one of ADC_SMPR_SMP_3CYC etc
} adc_freq_config_t;

float adc_setup(uint16_t, uint32_t, uint8_t *,uint32_t);
uint32_t adc_stop(void);
void adc_start(void);

float adc_get_sample_rate(uint16_t, uint32_t);


// Internal, but use at your own risk
void adc_apply_freq_config(adc_freq_config_t *);
#define ADC_RESOLUTION_NBITS 8 //!< How many bits of resolution we're using
#define ADC_RESOLUTION_CR1 ADC_CR1_RES_8BIT //!< What the STM32 wants to see for our resolution


#define ADC_PRESCALER_8KHZ 104 //!< The prescaler needed to get 8kHz
#define ADC_PERIOD_8KHZ 49 //!< The period needed to get 8kHz


#endif

/** \} */
