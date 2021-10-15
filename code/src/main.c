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
#include "leds.h"
#include "syscalls.h"
#include "tamo_state.h"


/*
 * First, a dirty hack to get our version string set up.
 */

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

/**
 * \brief Set up the DAC waveform to emit when bored
 */
static void register_dac_waveform(void) {
#define WAVEFORM_LEN 256
  uint8_t buf[256];
  int i;

  for (i = 0; i < WAVEFORM_LEN/2; i++) {
    buf[i] = 2*i;
  }
  for (i = WAVEFORM_LEN/2; i < WAVEFORM_LEN; i++) {
    buf[i] = 2*(255-i);
  }

  dac_register_buf(buf, WAVEFORM_LEN);
#undef WAVEFORM_LEN
}

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
  dac_setup();
  register_dac_waveform();

  current_time = 0;
  tamo_state_init(&tamo_state, current_time);

  while (1) {
    // Run this loop at about 10Hz, and poll for inputs.  (Huge antipattern!)
    for (int j = 0; j < 10; j++) {
      bool user_present = button_poll();

      if (tamo_state_update(&tamo_state, current_time, user_present)) {
        logline(LEVEL_INFO, "Transition to %s: %d",
		tamo_emotion_name(tamo_state.current_emotion), user_present);

	switch(tamo_state.current_emotion) {
	case TAMO_BORED:
	    dac_start(1152, 500);
	    break;
	default:
	  dac_stop();
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
