/**
 * \file main.c
 * \brief Main loop and helper functions
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "logging.h"

#include "system_clock.h"
#include "console.h"
#include "buttons.h"
#include "dac.h"
#include "adc.h"
#include "leds.h"
#include "eol_commands.h"

#include "syscalls.h"
#include "tamo_state.h"
#include "sin_gen.h"
#include "pi_reciter.h"
#include "dtmf.h"
#include "packet.h"


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

/**
 * States for our DTMF modem state machine
 *
 * The DTMF state machine controls both the modulation and the
 * demodulation.  When it "hears" a digit, it stops the DAC, and
 * prepares to go to the next digit once it's clear.  However, there
 * are race conditions, where a tone_start callback for the same
 * symbol can happen after the DAC was stopped.
 *
 * We therefore need a buffer state to handle that case.
 */
typedef enum tone_modem_state {
                               MODEM_IDLE = 0, //!< Not currently doing DTMF
                               MODEM_WAITING_SEND, //!< Triggered for send to start
                               MODEM_SENDING, //!< Modulating a symbol
                               MODEM_WAITING_STOP, //!< Tone detected, waiting for complete stop
                               MODEM_DONE, //!< Modem completed symbol
                               MODEM_RESTART, //!< Modem needs to restart
} tone_modem_state_t;

////////////////////////////////////////////////////////////
// Globals

#define DAC_WAVEFORM_LEN 1024
static uint8_t dac_buf[DAC_WAVEFORM_LEN]; //!< The waveform to emit when bored
static float dac_sample_rate; //!< The sampling rate of the DAC

#define ADC_NUM_SAMPLES 400 //!< Number of samples to capture, Double-buffer means this gets halved
static uint8_t adc_buf[ADC_NUM_SAMPLES];
static float adc_sample_rate; //!< The sampling rate of the ADC

static uint8_t modem_state;

static uint32_t console_callbacks_count;

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

  //  logline(LEVEL_DEBUG, "filling DTMF buffer: %d / %d  (%d Sps)", (int)f0, (int)f1, (int)dac_sample_rate);

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
  logline(LEVEL_INFO, "DAC sampling rate: %ld", (int)dac_sample_rate);
}


static void tone_start_next_digit(void) {
  float f_row, f_col;
  uint8_t next_digit;
  dtmf_status_t dtmf_stat;

  if (modem_state == MODEM_RESTART) {
    pi_reciter_reset();
  }

  next_digit = pi_reciter_next_digit();

  dtmf_stat = dtmf_get_tones(next_digit, &f_row, &f_col);

  logline(LEVEL_DEBUG_NOISY, "tone_start_next_digit: %d: Next digit will be: %c: %d/%d",
          modem_state, next_digit, (int)f_row, (int)f_col);
  if (DTMF_OKAY != dtmf_stat) {
    logline(LEVEL_ERROR, "tone_start_next_digit: Couldn't populate tones for symbol '%c' ", next_digit);
    //!< \todo Decode DTMF errors
    pi_reciter_reset();
    next_digit = pi_reciter_next_digit();
  }

  modem_state = MODEM_SENDING;

  // Just blast over the existing buffer while it's on, the glitches
  // don't matter to us here.
  fill_dtmf_waveform_buf(f_row,f_col);
  // Reinitialize DAC DMA buffer, in case it's been reset by the EOL code
  dac_waveform_setup();
  dac_start();
  adc_start();
}

static void tone_stop(void) {
  modem_state = MODEM_IDLE;
  dac_stop();
  adc_stop();
}


////////////////////////////////////////////////////////////
// Callbacks

/**
 * Callback for packet decodes
 */
static void packet_handler(uint8_t *payload, uint16_t payload_len,
                           uint8_t addr, uint8_t control,
                           uint8_t fcs_match)
{
  payload[payload_len] = 0; // DANGER
  logline(LEVEL_DEBUG_NOISY, "Got a payload packet: %02x/%02x: %d bytes cksum_match=%d, %s",
          addr, control, payload_len, fcs_match, payload);

  if (!fcs_match) return;

}

static void packet_too_long(uint8_t *buf, uint16_t buf_len,
                           uint8_t addr, uint8_t control,
                           uint8_t fcs_match)
{
  logline(LEVEL_DEBUG_NOISY, "Got a too-long-packet: %02x %02x %02x %02x%02x (bytes seen=%ld)",
          buf[0], buf[1], buf[2], buf[3], buf[4], console_callbacks_count );
}


static void packet_interrupted(uint8_t *buf, uint16_t buf_len,
                               uint8_t addr, uint8_t control,
                               uint8_t fcs_match)
{
  logline(LEVEL_DEBUG_NOISY, "Got an interrupted packet, after %d bytes: %02x %02x %02x %02x%02x",
          buf_len, buf[0], buf[1], buf[2], buf[3], buf[4] );
}


#define SERBUFLEN 2048
static uint8_t serbuf[SERBUFLEN];
static uint8_t *serbuf_head;
static uint8_t *serbuf_tail;
/**
 * Callback for serial console inputs
 */
static void console_line_handler(char *line, uint32_t line_len) {
  // Reset cursor if we can
  if (serbuf_head == serbuf_tail) {
    serbuf_head = serbuf;
    serbuf_tail = serbuf;
  }

  if (serbuf_tail + line_len - serbuf > SERBUFLEN) {
    console_dumps("Serial buffer overflow!");
    serbuf_head = serbuf;
    serbuf_tail = serbuf;
    return;
  }

  memcpy(serbuf_tail, line,line_len);
  serbuf_tail += line_len;

  console_callbacks_count += line_len;
}


/**
 * Callback for DTMF tone stop
 */
static void dtmf_tone_stop_cb(uint8_t sym, float ms) {
  pi_reciter_rx_state_t rx_state;
  uint8_t expected = pi_reciter_next_digit();

  //logline(LEVEL_INFO, "\t\tTone stop (%d): %c / %d ms expect %c",
  //       modem_state, sym, (int)(1000*ms), expected);

  if ((modem_state != MODEM_SENDING) && (modem_state != MODEM_WAITING_STOP)) {
    return;
  }

  rx_state = pi_reciter_rx_digit(sym);

  if (PI_RECITER_OKAY != rx_state) {
    logline(LEVEL_ERROR, "Got incorrect digit or am exhausted: got %c, expected %c, ms=%d",
            sym, expected, (int)(ms*1000));

    modem_state = MODEM_RESTART;
  } else {
    logline(LEVEL_INFO, "Pi: %c okay, will advance", sym);
    modem_state = MODEM_DONE;
  }


}


/**
 * Callback for DTMF tone detection start
 */
static void dtmf_tone_start_cb(uint8_t sym, float val) {
  if (0) // Have a reference to sym and val to make unused-vars happy
    logline(LEVEL_INFO, "\t\tTone start (%d) : %c: %d", modem_state, sym, (int)(val*1000));

  if (modem_state == MODEM_IDLE) return; // Ignore spurious callbacks when idle

  if (modem_state != MODEM_SENDING) return; // short-circuit races

  modem_state = MODEM_WAITING_STOP;

  // Stop sending digits so we can move on
  //logline(LEVEL_DEBUG, "Turning off tone now");
  dac_stop();
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
  static uint8_t packet_rx_buf[1024]; //!< Buffer for the packet handler to use

  float dtmf_threshold = 0.5;

  console_callbacks_count = 0;

  adc_config_t adc_config = {
                             .prescaler = ADC_PRESCALER_8KHZ,
                             .period = ADC_PERIOD_8KHZ,
                             .buf = adc_buf,
                             .buflen = ADC_NUM_SAMPLES,
                             .double_buffer = 1,
                             .n_channels = 1,
                             .channels = {0},
                             .double_buffer = 1,
                             .sample_width = 1,
                             .cb = dtmf_process,
  };

  // Only variables declaration/definitions above this line
  //////////////////////////////////////////////////
  // Critical init section /////////////////////////
  //

  system_clock_setup();

  // LEDs before anything else, so we can use them anywhere.
  led_setup();
  led_green_on();

  memset(console_rx_buffer, 0, 1024);
  console_setup(&console_line_handler, console_rx_buffer, 8);
  log_forced("TamoDevBoard startup, version " xstr(ARGALI_VERSION) " Compiled " __TIMESTAMP__);
  parser_setup(eol_command_handle, packet_rx_buf, 1024);
  parser_register_too_long_cb(&packet_too_long);
  parser_register_pkt_interrupted_cb(&packet_interrupted);

  //
  // End critical init section /////////////////////
  //////////////////////////////////////////////////

  // Less critical setup starts here

  // Misc UI elements
  button_setup();

  // DAC
  dac_waveform_setup();

  // ADC
  adc_sample_rate = adc_setup(&adc_config);
  logline(LEVEL_INFO, "Configured ADC at %d samples per second",
	  (uint32_t)adc_sample_rate);

  // Time initialization
  current_time = 0;

  // Set up the Tamo state machine
  tamo_state_init(&tamo_state, current_time);

  // Get ready to recite digits of pi
  pi_reciter_init();

  // And then set up DTMF decoding
  modem_state = MODEM_IDLE;
  dtmf_init(adc_sample_rate, dtmf_threshold, dtmf_tone_start_cb, dtmf_tone_stop_cb);


  console_dumps("\n\nSTARTUP\n\n");

  ////////////////////////////////////////////////////////////
  // Main Loop
  //
  while (1) {
    // Run this loop at about 10Hz, and poll for inputs.  (Huge antipattern!)
    for (int j = 0; j < 10; j++) {

      for ( ; serbuf_head < serbuf_tail; serbuf_head++) {
        packet_rx_byte(*serbuf_head);
      }

      bool user_present = button_poll();

      if ((modem_state == MODEM_RESTART) || (modem_state == MODEM_DONE)) {

        if (modem_state == MODEM_RESTART) {
          logline(LEVEL_DEBUG, "Reseting pi reciter");
          tone_stop();
          pi_reciter_reset();
        }

        logline(LEVEL_DEBUG, "Main loop: Advancing digit");
        modem_state = MODEM_WAITING_SEND;
        tone_start_next_digit();
      }

      if (user_present) {
        console_dumps("up");
      }


      //////////////////////////////
      // Drive TamoDevBoard machine
      if (tamo_state_update(&tamo_state, current_time, user_present)) {
        logline(LEVEL_INFO, "Transition to %s: %d",
		tamo_emotion_name(tamo_state.current_emotion), user_present);

	switch(tamo_state.current_emotion) {
	case TAMO_BORED:
          if (modem_state == MODEM_IDLE) {
            // Start on button press for now
            modem_state = MODEM_WAITING_SEND;
            logline(LEVEL_DEBUG, "Main loop: Starting modem");
            tone_start_next_digit();
          } else {
            logline(LEVEL_DEBUG, "Main loop: Modem state: %d", modem_state);
          }
	  break;
	default:
          tone_stop();
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
