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
 *
 * A note on buflen: in this structure, it's the size of your buffer
 * in bytes.  It's just how much memory is available.  The layers
 * below this call will use the number of channels and the sample
 * width to determine how many points fit within it, and submit the
 * DMA requests appropriately.
 *
 * If you are using double-buffering, the above goes twice for you:
 * the final DMA call will split your buffer in half, and use one half
 * for each of the two buffers.
 *
 * So, to sort out how much RAM you need, just do:
 *
 * n_pts_desired * sample_width * n_channels * (double_buffered?2:1)
 *
 * It is unclear if double-buffering wants a word alignment for the
 * split; it seems like the architecture takes care of these problems
 * for us.
 *
 * Clocking of channel conversions: this is poorly documented in the
 * manuals.  It turns out that the sampling is done in sequence after
 * the trigger, so your signals will walk off by
 * (r+ADC_SMPR_SMP_N)/ADCCLK seconds between each channel read, where
 * r is the number of bits of resolution you have selected.  (p344 and
 * p346).  The ADCCLK can be adjusted with the adcclk_prescaler value
 * in this struct (as the prescaler value to use, but it has to be one
 * of 2,4,6, or 8.  The sample time value is passed in as a value to
 * use, but must be one of 3,15,28,56,94,112,144,480.
 *
 * We do not currently support anything but 8 and 12 bit resolution.
 *
 * We do not currently support setting the data to be left aligned.
 *
 * We do not currently support injected conversions.
 *
 * We do not currently support per-channel conversion times.
 */
typedef struct adc_config {
  // Timer settings
  uint16_t prescaler; //!< The prescaler to use for the timer, see timer_setup_adcdac()
  uint32_t period; //!< The period to use for the timer

  // DMA settings
  uint8_t *buf;  //!< A pointer to the buffer to fill
  uint16_t buflen; //!< The length of the buffer, in bytes
  uint8_t double_buffer; //!< Flag to enable/disable double-buffering

  // ADC settings
  uint8_t n_channels; //!< The number of channels to sample
  uint8_t channels[16]; //!< The order in which to sample the channels
  uint8_t sample_width; //!< 1 for 8b samples, 2 for 12b samples
  uint8_t adcclk_prescaler; //!< Prescaler for ADCCLK, used to drive conversions, p363
  uint16_t adc_sample_time; //!< Value of ADC_SMPR_SMP to use (see p356)

  adc_buffer_cb cb; //!< Callback to call when buffers get filled
} adc_config_t;

float adc_setup(adc_config_t *);
uint32_t adc_stop(void);
void adc_start(void);

float adc_get_sample_rate(void);
float adc_get_interchannel_time(void);


#define ADC_PRESCALER_8KHZ 104 //!< The prescaler needed to get 8kHz
#define ADC_PERIOD_8KHZ 49 //!< The period needed to get 8kHz


#endif

/** \} */
