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
#include "dtmf.h"



// First, a dirty hack to get our version string set up.
#define xstr(x) str(x)  //!< Stage 2 of a janky two-stage macro-stringifier
#define str(x) #x       //!< Stage 1 of a janky two-stage macro-stringifier
#ifndef ARGALI_VERSION
#define ARGALI_VERSION UNKNOWN  //!< Default version if none given (No quotes)
#endif

////////////////////////////////////////////////////////////
// Globals

#define DAC_WAVEFORM_LEN 1000
static uint8_t dac_buf[DAC_WAVEFORM_LEN]; //!< The waveform to emit when bored
static float dac_sample_rate; //!< The sampling rate of the DAC

#define ADC_NUM_SAMPLES 512
static uint8_t adc_buf[ADC_NUM_SAMPLES];
static float adc_sample_rate; //!< The sampling rate of the ADC

static uint8_t got_button_down;

////////////////////////////////////////////////////////////
// Misc functions

/**
 * Fill a buffer with a DTMF waveform, using the less-pleasant
 * sin_gen_generate_fill()
 */
static void fill_dtmf_waveform_buf(float f0, float f1) {
  sin_gen_result_t res;
  sin_gen_request_t req;

  uint8_t working_buf[DAC_WAVEFORM_LEN];

  logline(LEVEL_DEBUG, "filling DTMF buffer: %d / %d  (%d Sps)", (int)f0, (int)f1, (int)dac_sample_rate);

  //////////////////////////////
  // Generate tone f0 at full amplitude in our working buffer
  res = sin_gen_populate(&req, working_buf, DAC_WAVEFORM_LEN, f0, dac_sample_rate);
  if (SIN_GEN_OKAY != res) {
    logline(LEVEL_ERROR, "Failed to populate sin_gen request, bailing on DAC setup: %s!",
	    sin_gen_result_name(res));
    return;
  }
  req.scale = 2; // Turn it down a little

  res = sin_gen_generate_fill(&req);
  if (SIN_GEN_OKAY != res) {
    logline(LEVEL_ERROR, "Failed to generate sine tone of %ld Hz, bailing on DAC setup: %s!",
	    f0, sin_gen_result_name(res));
    return;
  }


  //////////////////////////////
  // Generate tone f1 at full amplitude in our dac buffer
  res = sin_gen_populate(&req, dac_buf, DAC_WAVEFORM_LEN, f1, dac_sample_rate);
  if (SIN_GEN_OKAY != res) {
    logline(LEVEL_ERROR, "Failed to populate sin_gen request, bailing on DAC setup: %s!",
	    sin_gen_result_name(res));
    return;
  }
  req.scale = 2; // Turn it down a little

  res = sin_gen_generate_fill(&req);
  if (SIN_GEN_OKAY != res) {
    logline(LEVEL_ERROR, "Failed to generate sine tone of %ld Hz, bailing on DAC setup: %s!",
	    f1, sin_gen_result_name(res));
    return;
  }

  //////////////////////////////
  // And combine the two
  for (int i = 0; i < DAC_WAVEFORM_LEN; i++) {
    int16_t j = dac_buf[i] + working_buf[i];
    dac_buf[i] = j/2;
  }
}

/**
 * \brief Set up the DAC waveform to emit when bored
 */
static void dac_waveform_setup(void) {
  uint16_t prescaler = 24;
  uint32_t period = 49;

  dac_sample_rate = dac_get_sample_rate(prescaler, period);

  dac_setup(prescaler, period, dac_buf, DAC_WAVEFORM_LEN);
}


////////////////////////////////////////////////////////////
// Callbacks



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


static void button_up_cb(uint8_t sym, float ms) {
  float f_row, f_col;
  uint8_t next_digit;
  dtmf_status_t dtmf_stat;

  logline(LEVEL_INFO, "\t\t\t\tButton up: %c / %d ms", sym, (int)(1000*ms));

  got_button_down = 0;

  // Get the next digit and roll with it

  next_digit = pi_reciter_next_digit();

  dtmf_stat = dtmf_get_tones(next_digit, &f_row, &f_col);

  if (DTMF_OKAY != dtmf_stat) {
    logline(LEVEL_ERROR, "Couldn't populate tones for symbol '%c' ", next_digit);
    pi_reciter_reset();
  }

  fill_dtmf_waveform_buf(f_row,f_col);
  dac_start();
}



static void button_down_cb(uint8_t sym) {
  pi_reciter_rx_state_t rx_state;

  uint8_t expected = pi_reciter_next_digit();

  // Stop sending digits so we can move on
  dac_stop();

  got_button_down = 1;

  logline(LEVEL_INFO, "\t\t\t\tButton down: %c", sym);
  rx_state = pi_reciter_rx_digit(sym);

  if (PI_RECITER_OKAY != rx_state) {
    logline(LEVEL_ERROR, "Got incorrect digit or am exhausted: got %c, expected %c",
            sym, expected);
    pi_reciter_reset();

    button_up_cb('D', 0);
  }
}




////////////////////////////////////////////////////////////
// Main
//

/**
 * \brief The main loop.
 */
int main(void) {
  uint32_t current_time; //! \todo We still need to hook up the RTC.
  tamo_state_t tamo_state;

  static char console_rx_buffer[1024]; //!< Buffer for the console to use

  uint8_t next_digit;
  float f_row, f_col;

  dtmf_status_t dtmf_stat;
  float dtmf_threshold = 0.05;

  // Only variables above this line
  //////////////////////////////////////////////////
  // Critical init section /////////////////////////
  //

  system_clock_setup();

  console_setup(&console_line_handler, console_rx_buffer, 1024);
  log_forced("TamoDevBoard startup, version " xstr(ARGALI_VERSION) " Compiled " __TIMESTAMP__);

  //
  // End critical init section /////////////////////
  //////////////////////////////////////////////////

  // Less critical setup starts here

  // Misc UI elements
  led_setup();
  button_setup();

  // DAC
  dac_waveform_setup();

  // ADC
  adc_sample_rate = adc_setup(adc_buf, ADC_NUM_SAMPLES);
  logline(LEVEL_INFO, "Configured ADC at %d samples per second",
	  (uint32_t)adc_sample_rate);


  // Time initialization
  current_time = 0;

  // Set up the Tamo state machine
  tamo_state_init(&tamo_state, current_time);

  // Get ready to recite digits of pi
  pi_reciter_init();

  // And then set up DTMF decoding
  dtmf_init(adc_sample_rate, dtmf_threshold, button_down_cb, button_up_cb);
  // Prime DTMF decoding, will start first digit for now
  button_up_cb('D', 0);


  ////////////////////////////////////////////////////////////
  // Main Loop
  //
  while (1) {
    // Run this loop at about 10Hz, and poll for inputs.  (Huge antipattern!)
    for (int j = 0; j < 10; j++) {
      if (got_button_down) {
        // DAC is off, send a zero-length finish to start next tone
        dtmf_process(NULL, 0);
      }



      bool user_present = button_poll();

      if (user_present) {
        //////////////////////////////
        // Janky DTMF decode trigger for testing

        // Pause the ADC
        int offset = adc_pause();

        int i0 = ADC_NUM_SAMPLES - offset; // Seek to end of existing buffer
        dtmf_process(adc_buf, ADC_NUM_SAMPLES);
#if 1

        logline(LEVEL_DEBUG_NOISY, "Stopped ADC with at sample %d.  Unwrapping.", i0);
        printf("x = [ \n  ");
        for(int i = 0 ; i < ADC_NUM_SAMPLES; i++) {
          printf(" %3d,", adc_buf[(i0+i)%ADC_NUM_SAMPLES]);
          if (15 == (i % 16)) {
            printf("\n  ");
          }
        }
        printf("\n]\n");
#endif
        adc_unpause();
      }


      //////////////////////////////
      // Drive TamoDevBoard machine
      if (tamo_state_update(&tamo_state, current_time, user_present)) {
        logline(LEVEL_INFO, "Transition to %s: %d",
		tamo_emotion_name(tamo_state.current_emotion), user_present);

	switch(tamo_state.current_emotion) {
	case TAMO_BORED:
	  break;
	default:
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
