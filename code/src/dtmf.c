#include "dtmf.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#include "sin_gen.h"
#define M_PI 2*COS_THETA0
#endif

/**
 * \file dtmf.c
 * \brief A DTMF modulator/demodulator
 *
 * \defgroup dtmf DTMF modulator/demodulator
 * \addtogroup dtmf
 * \{
 *
 * When it's bored, TamoDevBoard likes to recite digits of pi.  It
 * does this by emitting them as DTMF out the DAC, and confirming that
 * they came in correctly on the ADC.  After it's heard itself say a
 * digit, it moves on to the next one.
 *
 * For decoding, we use the Goertzel algorithm, modified to support
 * non-integer bins.  This is a lot of magic math that gets it down to
 * a surprisingly small amount of processing.  Many thanks to ST for
 * publishing Design Tip DT0089, in which Andrea Vitali lays out this
 * extended version of Goertzel.
 *
 * You can find a much longer explanation than DT0089 in
 * code/notebooks/dtmf_filter_design.ipynb , where we re-derive some
 * key bits, and test out a python version of the implementation found
 * below.
 */


static const float dtmf_tones[8] = { 697,770, 852, 941,  // rows
			       1209, 1336, 1477, 1633 // cols
};

/**
 * The symbols we decode to.  Note that these are set up just like the
 * keypad, such that you index into them by grabbing the symbol at
 * 4*row_tone + col_tone
 */
static const uint8_t dtmf_symbols[] = "123A456B789C*0#D";

static dtmf_decoder_config_t dtmf_config; //!< Our configuration
static dtmf_decoder_state_t dtmf_state; //!< The current decoder state

/**
 * Fill in the given float pointers with the tones for the given symbol
 *
 * \param symbol The symbol to look up
 * \param row_tone[out] The frequency of the row tone
 * \param col_tone[out] the frequency of the column tone
 *
 * \returns DTMF_OKAY if things worked, otherwise
 * DTMF_SYMBOL_NOT_FOUND if the symbol was not found, or
 * DTMF_INVALID_INPUTS if a NULL pointer was passed in for either
 * float.
 */
dtmf_status_t dtmf_get_tones(uint8_t symbol, float *row_tone, float *col_tone) {
  if (!row_tone || !col_tone) {
    return DTMF_INVALID_INPUTS;
  }
  for (int i = 0; i < 16; i++) {
    if (symbol == dtmf_symbols[i]) {
      *row_tone = dtmf_tones[i/4];
      *col_tone = dtmf_tones[4 + i%4];
      return DTMF_OKAY;
    }
  }
  return DTMF_SYMBOL_NOT_FOUND;
}



/**
 * \brief Reset the internal state of the DTMF decoder
 *
 * Note that this resets all the cos/sin tables to zero, along with
 * the historic z values and the cur_symbol_dt.  It also sets
 * cur_symbol to DTMF_SYMBOL_NONE, to indicate that nothing has been
 * decoded.
 */
static void dtmf_reset_internals(void) {
  memset(&dtmf_state, 0, sizeof(dtmf_decoder_state_t));
  dtmf_state.cur_symbol = DTMF_SYMBOL_NONE;
}


/**
 * Initialize for DTMF decoding with the given parameters
 *
 * \param Fs The sampling rate, in samples per second ("Hz")
 * \param threshold The threshold to consider a tone a "hit" (see below)
 * \param down_cb The callback when a new tone is detected (button down)
 * \param up_cb The callback when a tone ends (button up)
 *
 * Threshold setting: you probably want this to be roughly 2/sqrt(N),
 * where N is the number of samples you're going to call
 * dtmf_process() with every time.  When in doubt, 0.2 is probably
 * safe.  For a lot (and I do mean a lot) more detail, see the
 * notebook for this file, code/notebooks/dtmf_filter_design.ipynb .
 * All of the math is worked out in there.
 *
 * The code in the callbacks should be treated with care.  You may
 * call them from inside an ISR, so be thoughtful about how much work
 * you do in them.
 */
void dtmf_init(float Fs, float threshold, dtmf_down_callback down_callback, dtmf_up_callback up_callback) {
  dtmf_reset_internals();

  memset(&dtmf_config, 0, sizeof(dtmf_decoder_config_t));
  dtmf_config.Fs = Fs;
  dtmf_config.threshold = threshold;
  dtmf_config.down_cb = down_callback;
  dtmf_config.up_cb = up_callback;

  // Populate our sine/cos tables
  for (int tone_no = 0; tone_no < 8; tone_no++) {
    float w = 2 * M_PI * dtmf_tones[tone_no]/dtmf_config.Fs;
    dtmf_state.cos_w_table[tone_no] = cosf(w);
    dtmf_state.sin_w_table[tone_no] = sinf(w);

    /*
    printf("DTMF populate: tone %d / %d Hz Fs=%d: w=%d cos_w=%d sin_w=%d\n",
           tone_no,
           (int)(dtmf_tones[tone_no]),
           (int)Fs,
           (int)(1000*w),
           (int)(1000*dtmf_state.cos_w_table[tone_no]),
           (int)(1000*dtmf_state.sin_w_table[tone_no])
           );
    */
  }
}

/**
 * Given a row and column, return the symbol at that spot
 *
 * \return A DTMF symbol, or DTMF_SYMBOL_NONE on invalid inputs
 */
static uint8_t decode_symbol(uint8_t row, uint8_t col) {
  uint16_t i = 4*row + col;

  if (i > 16) return DTMF_SYMBOL_NONE;

  return dtmf_symbols[i];
}


/**
 * Wraps all the DTMF state transition logic
 *
 * This gets called any time a new symbol is found.
 *
 * Doing it this way centralizes a lot of logic that is otherwise
 * duplicated all over the place.
 */
static void dtmf_sym_decoded(uint8_t new_symbol, float dt) {
  // If symbol matches what we have, just increment dt and return
  if (new_symbol == dtmf_state.cur_symbol) {
    dtmf_state.cur_symbol_dt += dt;
    return;
  }

  // Otherwise, if we have a valid symbol, do an up state
  if (DTMF_SYMBOL_NONE != dtmf_state.cur_symbol) {
    dtmf_config.up_cb(dtmf_state.cur_symbol, dtmf_state.cur_symbol_dt);
  }

  // Then set our new state
  dtmf_state.cur_symbol = new_symbol;
  dtmf_state.cur_symbol_dt = dt;

  // And if it's a valid symbol, do a down_cb
  if (DTMF_SYMBOL_NONE != new_symbol) {
    dtmf_config.down_cb(new_symbol);
  }
  return;
}


/**
 * Process a single buffer that continues the previous state
 *
 * \param buf A buffer of uint8_t samples from the DAC
 * \param buflen The length of the buffer (0 to indicate end of data)
 *
 * Calling this with a buflen of zero will cause any (needed) final
 * up_callback() to be called.  This is useful when you shut down the
 * ADC and want to wrap up a single session of symbol decoding.
 *
 * Generally, you want to call this with a smaller number of samples,
 * on the order of 200.
 *
 * You want the number of samples in here to represent less than half
 * the length of a single valid button-down event in your system or
 * about the same length as the minimum button-up time between symbols
 * (DTMF specification is 45ms down, with 10ms between them, so 10ms
 * buffers can work).
 *
 * This will process the buffer through the IIR filter, then check its
 * thresholds at the end.  If it believes a button has been pushed
 * down it, it does the following:
 *
 * * If a different valid symbol is in dtmf_state.cur_symbol, calls
 *   up_callback()
 *
 * * Places the new symbol in dtmf_state.cur_symbol and resets
 *    cur_symbol_dt to one buffer length.
 *
 * * Calls down_callback()
 *
 * If the symbol detected matches dtmf_state.cur_symbol, just
 * increments dtmf_state.cur_symbol_dt.
 *
 * If it finds no symbol, it does the following:
 *
 * * If a valid symbol is in dtmf_state.cur_symbol, calls up_callback()
 *
 * * Resets dtmf_state.cur_symbol to DTMF_SYMBOL_NONE
 */
void dtmf_process(const uint8_t *buf, uint16_t buflen) {
  float dt;

  // If buflen == 0 and valid cur_symbol, call up_callback() and reset
  if (buflen == 0) {
    dtmf_sym_decoded(DTMF_SYMBOL_NONE, 0);
    return;
  }

  ////////////////////////////////////////////////////////////
  // Run the filter to find the strongest row and column tone

  uint8_t best_row = 0xFF;
  float best_row_mag = 0;

  uint8_t best_col = 0xFF;
  float best_col_mag = 0;

  for (int tone_no = 0; tone_no < 8; tone_no++) {
    float z2 = 0;
    float z1 = 0;

    float cos_w = dtmf_state.cos_w_table[tone_no];
    float sin_w = dtmf_state.sin_w_table[tone_no];

    float res_i, res_q; // result values
    float mag; // magnitude

    for (int i = 0; i < buflen; i++) {
      //! \todo Remove this hard-coded offset and amplitude
      float x = (buf[i] - 127.0)/127.0; // Current sample
      float z0 = x + (2*cos_w * z1) - z2;
      z2 = z1;
      z1 = z0;
    }

    //////////////////////////////
    // Get magnitude
    res_i = z1 * cos_w - z2;
    res_q = z1 * sin_w;

    mag = res_i*res_i + res_q*res_q;
    mag /= buflen/2;  // Apply correction factor

    // printf("dtmf_process tone %d: %d\n", tone_no, (int)(1000*mag));
    //////////////////////////////
    // See if it's a winner
    if (tone_no < 4) {
      // Running a row tone
      if (mag > best_row_mag) {
	best_row_mag = mag;
	best_row = tone_no;
      }
    } else {
      // Running a column tone
      if (mag > best_col_mag) {
	best_col_mag = mag;
	best_col = tone_no;
      }
    }
  }


  ////////////////////////////////////////////////////////////
  // Execute the appropriate transitions

  dt = buflen/dtmf_config.Fs; // First get our time elapsed, though.

  // Fix up threshold to use mag-squared and avoid sqrt
  float th = dtmf_config.threshold * dtmf_config.threshold;

  if ((best_row_mag < th) || (best_col_mag < th)) {
    dtmf_sym_decoded(DTMF_SYMBOL_NONE, dt);
    return;
  }

  ////////////////////////////////////////////////////////////
  // Decode the symbol
  uint8_t symbol_found = decode_symbol(best_row, best_col-4);

  ////////////////////////////////////////////////////////////
  // Do state transitions
  dtmf_sym_decoded(symbol_found, dt);
  return;
}

/** \} */ // End doxygen group
