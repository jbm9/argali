/**
 * \file main.c
 * \brief Main loop and helper functions
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "system_clock.h"
#include "logging.h"
#include "console.h"
#include "buttons.h"
#include "dac.h"
#include "adc.h"
#include "leds.h"
#include "syscalls.h"
#include "tamo_state.h"
#include "sin_gen.h"
#include "pi_reciter.h"


// First, a dirty hack to get our version string set up.
#define xstr(x) str(x)  //!< Stage 2 of a janky two-stage macro-stringifier
#define str(x) #x       //!< Stage 1 of a janky two-stage macro-stringifier
#ifndef ARGALI_VERSION
#define ARGALI_VERSION UNKNOWN  //!< Default version if none given (No quotes)
#endif

 /**
 * \defgroup 00mainloop Main loop
 * \{
 *
 * This is the main loop; it runs a busy loop that manages polling for
 * buttons, cycling through LED states, and occasionally prints stuff
 * to the console.
 */

static void console_line_handler(char *line, uint32_t line_len) {
  line_len = line_len;  // Make the unused parameter warning go away
  logline(LEVEL_INFO, "Got line: %s", line);
}

static uint8_t dac_buf[256]; //!< The waveform to emit when bored

static float dac_sample_rate; //!< The sampling rate of the DAC
static float adc_sample_rate; //!< The sampling rate of the ADC

/**
 * \brief Set up the DAC waveform to emit when bored
 */
static void dac_waveform_setup(void) {
#define WAVEFORM_LEN 256
  int i;
  uint16_t prescaler = 9;
  uint32_t period = 2;

  dac_sample_rate = dac_get_sample_rate(prescaler, period);

  sin_gen_result_t res;
  sin_gen_request_t req;

  int f_tone = 20000;

  res = sin_gen_populate(&req, dac_buf, WAVEFORM_LEN, f_tone, dac_sample_rate);
  if (SIN_GEN_OKAY != res) {
    logline(LEVEL_ERROR, "Failed to populate sin_gen request, bailing on DAC setup: %s!",
	    sin_gen_result_name(res));
    return;
  }
  req.scale = 2;

  res = sin_gen_generate(&req);
  if (SIN_GEN_OKAY != res) {
    logline(LEVEL_INFO, "DAC sample rate: %ld sps", (int)dac_sample_rate);
    logline(LEVEL_ERROR, "Failed to generate sine tone of %ld Hz, bailing on DAC setup: %s!",
	    f_tone, sin_gen_result_name(res));
    return;
  }

  logline(LEVEL_INFO, "DAC sample rate: %ld, expected freq: %ld Hz (%d samples long)",
	  (int)dac_sample_rate,
	  (int)(dac_sample_rate/req.result_len),
	  req.result_len);
  dac_setup(prescaler, period, dac_buf, req.result_len);
#undef WAVEFORM_LEN
}

#define ADC_NUM_SAMPLES 512
static uint8_t adc_buf[ADC_NUM_SAMPLES];

/**
 * \brief The main loop.
 */
int main(void) {
  uint32_t current_time; //! \todo We still need to hook up the RTC.
  tamo_state_t tamo_state;

  static char console_rx_buffer[1024]; //!< Buffer for the console to use

  system_clock_setup();

  console_setup(&console_line_handler, console_rx_buffer, 1024);
  log_forced("TamoDevBoard startup, version " xstr(ARGALI_VERSION) " Compiled " __TIMESTAMP__);
  led_setup();
  button_setup();
  dac_waveform_setup();

  current_time = 0;
  tamo_state_init(&tamo_state, current_time);
  pi_reciter_init();

  uint32_t desired_rate = 10000;
  adc_sample_rate = adc_setup(adc_buf, ADC_NUM_SAMPLES, desired_rate);
  logline(LEVEL_INFO, "Configured ADC at %d samples per second",
	  (uint32_t)adc_sample_rate);
  if (0 == adc_sample_rate) {
    logline(LEVEL_ERROR, "Could not set up ADC to desired rate of %d Hz!",
	    (uint32_t)desired_rate);
  }

  while (1) {
    // Run this loop at about 10Hz, and poll for inputs.  (Huge antipattern!)
    for (int j = 0; j < 10; j++) {
      bool user_present = button_poll();
      if (tamo_state_update(&tamo_state, current_time, user_present)) {
        logline(LEVEL_INFO, "Transition to %s: %d",
		tamo_emotion_name(tamo_state.current_emotion), user_present);

	switch(tamo_state.current_emotion) {
	case TAMO_BORED:
	  uint8_t next_digit = pi_reciter_next_digit();
	  dac_start();
	  break;
	default:
	  dac_stop();
	  break;
	}
      }
      switch (tamo_state.current_emotion) {
      case TAMO_LONELY: // blink red at 5Hz when lonely
	led_blue_off();
        led_red_on();
        break;

      case TAMO_HAPPY: // Solid blue when happy
	led_red_off();
	led_blue_on();
        break;

      case TAMO_BORED: // Blink blue at 2Hz when bored
        if (j % 5 == 0) {
          led_blue_toggle();
        }
        break;
      case TAMO_UNKNOWN:
      default:
        led_blue_toggle();
        led_red_toggle();
        break;
      }
      _delay_ms(100);
    }
    // Now increment our second
    current_time++;
  }
}

/** \} */ // End doxygen group
