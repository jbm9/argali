#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
/**
 * \file dtmf.h
 * \brief Header for DTMF modem
 *
 * \addtogroup dtmf
 * \{
 */


typedef void (*dtmf_down_callback)(uint8_t); //!< callback when a tone is first hit
typedef void (*dtmf_up_callback)(uint8_t, float); //!< callback when a tone stops

/**
 * The user-configurable bits of the DTMF Decoder
 */
typedef struct dtmf_decoder_config {
  float Fs; //!< Sample rate
  float threshold; //!< Minimum amplitude to consider a "hit"

  dtmf_down_callback down_cb; //!< callback when a tone is first hit
  dtmf_up_callback up_cb; //!< callback when a tone stops
} dtmf_decoder_config_t;

#define DTMF_SYMBOL_NONE 0xff //!< Used to indicate there is no current symbol

/**
 * The internal state of the DTMF decoder
 *
 * Note that most of these are 8-long arrays: these are coindexed with
 * the dtmf_tones array found in dtmf.c.  The first four correspond to
 * row tones, and the second four to the column tones.
 */
typedef struct dtmf_decoder_state {
  float cos_w_table[8]; //!< The cos(w) used in the filter, for each tone
  float sin_w_table[8]; //!< The sin(w) used in the filter, for each tone

  uint8_t cur_symbol; //!< Symbol we are currently in dtmf_down for
  float cur_symbol_dt; //!< How long we've been in that state
} dtmf_decoder_state_t;


/**
 * Status codes used in DTMF tone fetching
 */
typedef enum dtmf_status {
			  DTMF_OKAY = 0,  //!< The symbol was found
			  DTMF_SYMBOL_NOT_FOUND, //!< The symbol was not in our table
			  DTMF_INVALID_INPUTS, //!< A NULL pointer was passed in
} dtmf_status_t;

void dtmf_init(float, float, dtmf_down_callback, dtmf_up_callback);
void dtmf_process(const uint8_t *, uint16_t); // Process a buffer


dtmf_status_t dtmf_get_tones(uint8_t, float *, float *);
/** \} */ // End doxygen group
