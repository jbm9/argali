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


typedef void (*adc_buffer_cb)(const uint8_t *, uint16_t); //!< An ADC DMA callback

/**
 * An ADC Configuration
 *
 * This is used to bring up the ADC in both the main application mode
 * and EOL mode.
 *
 * It can be used to do most non-injected ADC tasks in the background
 * reasonably efficiently.
 *
 * See the codepaths that call adc_setup() to get examples of its use.
 */
typedef struct adc_config {
  // Timer settings
  uint16_t prescaler; //!< The prescaler to use for the timer, see timer_setup_adcdac()
  uint32_t period; //!< The period to use for the timer

  uint8_t *buf;  //!< A pointer to the buffer to fill
  uint16_t buflen; //!< The length of the buffer, in bytes
  uint8_t double_buffer; //!< Flag to enable/disable double-buffering

  uint8_t n_channels; //!< The number of channels to sample
  uint8_t channels[16]; //!< The order in which to sample the channels
  uint8_t sample_width; //!< 1 for 8b samples, 2 for 12b samples

  adc_buffer_cb cb; //!< Callback to call when buffers get filled
} adc_config_t;

float adc_setup(adc_config_t *);
uint32_t adc_stop(void);
void adc_start(void);

float adc_get_sample_rate(uint16_t, uint32_t);


#define ADC_PRESCALER_8KHZ 104 //!< The prescaler needed to get 8kHz
#define ADC_PERIOD_8KHZ 49 //!< The period needed to get 8kHz


#endif

/** \} */
