#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
/**
 * \file sin_gen.h
 * \brief Header for sine generator
 *
 * \addtogroup sin_gen
 * \{
 */

/**
 * \brief A sine wave generation result
 *
 * Generating a sine wave table has a few different failure modes,
 * which are captured here.
 */
typedef enum sin_gen_result {
			   SIN_GEN_OKAY=0,  //!< Successful generation

			   SIN_GEN_INVALID, //!< Invalid request (null buffer or 0 buflen)
			   SIN_GEN_UNDERSAMPLED,  //!< Sampling frequency too low for requested frequency
			   SIN_GEN_TOO_SHORT, //!< Buffer is too short to fit a full sine wave
} sin_gen_result_t;

#define SIN_THETA0 0.0 //!< Theta0 for sine waves, 0 radians
#define COS_THETA0 1.57079632679489661923  //!< Theta0 for cosine; could be M_PI_2 with math.h

/**
 * \brief A sine wave generation request/result
 *
 * Please see sin_gen_generate() for how to use these.
 */
typedef struct sin_gen_request {
  uint8_t *buf; //!< The buffer to fill, must be non-NULL
  uint16_t buflen; //!< The length of the buffer
  float theta0; //!< (radians) The initial phase angle (0 for sin, pi/2 for cos)

  uint32_t f_tone; //!< The frequency being requested
  uint32_t f_sample; //!< The sampling rate (usually your DAC output frequency)

  uint16_t result_len; //!< OUTPUT: How long a single wave is, in samples
  float phase_error; //!< OUTPUT: (radians) How many radians of error are dropped at the result_len wraparound
} sin_gen_request_t;


uint8_t sin_gen_sin(float);
sin_gen_result_t sin_gen_populate(sin_gen_request_t *, uint8_t *, uint16_t, uint32_t, uint32_t);
sin_gen_result_t sin_gen_generate(sin_gen_request_t *);

/** \} */ // End doxygen group
