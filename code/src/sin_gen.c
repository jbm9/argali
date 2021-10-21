#include "sin_gen.h"
#include <string.h>

/**
 * \file sin_gen.c
 * \brief A sine wave generator/sampler
 *
 * \defgroup sin_gen Sine wave generator (8 bit only)
 * \addtogroup sin_gen
 * \{
 *
 * If you have free space and compute on your product, just use libm
 * and its much better implementations of sin.  Otherwise, you can use
 * the approximations given here.
 *
 * This module implements a sine wave sampler, geared towards the
 * STM32 DAC.  In light of that, it only returns unsigned 8b values,
 * which is what the DAC expects as input.  It will fill a buffer with
 * appropriate points to give a decent approximation of a sine wave of
 * a given frequency, allowing that buffer to be DMA'd out the DAC.
 *
 * The frequency of the resulting sine wave isn't absolute: it depends
 * on the sampling frequency of the rest of the system.  For example,
 * if we simply output a buffer containing a single complete sine wave
 * in 1,000 positions, then send that out the DAC at 100kHz, we get a
 * 100Hz tone.  If we send it out at 50kHz, we only get a 50Hz tone.
 *
 * To use this, you must first populate a sin_gen_request_t, which
 * gives the generator all the state it needs to have to make your
 * sine wave.  There is a helper function for this boilerplate,
 * sin_gen_populate() below.  sin_gen_populate() will return a
 * sin_gen_result_t error code (see the description for details).
 *
 * Once the sin_gen_request_t struct is prepared, you then call
 * sin_gen_generate() with it.  This will do some quick validation of
 * the input, and return a sin_gen_result_t in the case of any errors.
 * After validating your request, it will fill in the buffer with one
 * sine wave at the requested frequency.  It fills out the result_len
 * member of your request structure, so you know how long it actually
 * is.
 *
 * Note that sin_gen_generate() may return SIN_GEN_TOO_SHORT if your
 * buffer was not long enought to contain a full wave at the sampling
 * rate you specified.  In this case, it does fill in your entire
 * buffer, and assumes that you know what you're doing.  See the
 * documentation for sin_gen_generate() for more about this.
 *
 * This also provides you with a phase error in the final sample.
 * This is how many radians off from zero the last sample is.  You may
 * wish to use this if you run into SIN_GEN_TOO_SHORT but cannot
 * adjust parameters to get a better fit.
 *
 * There is a lot more documentation in sin_gen_generate(), so that is
 * probably a good next place to look for information on this module.
 *
 * Also, note that this only works for integer frequencies.  This
 * should be adequate, but it may be desirable to rework it to use
 * floating point tone frequencies.  The sampling rate will always be
 * an integer due to the design of the microcontroller, but there is
 * no fundamental restriction on the tone you're generating.
 */


#define SINE_TABLE_LENGTH 256 //!< How many samples are in our quadrant: must divide UINT16_MAX!
/**
 * A quarter wave of sin, in uint8_t
 *
 * We only need 8b of precision for the DAC, so we just put in a
 * quarter wave of sin here, then the generator has to wrap it around
 * appropriately for each quadrant.
 */
static const uint8_t sin_table[256] = {
    0,   0,   1,   2,   3,   3,   4,   5,   6,   7,   7,   8,   9,  10,  10,  11,
   12,  13,  14,  14,  15,  16,  17,  17,  18,  19,  20,  21,  21,  22,  23,  24,
   24,  25,  26,  27,  27,  28,  29,  30,  30,  31,  32,  33,  34,  34,  35,  36,
   37,  37,  38,  39,  39,  40,  41,  42,  42,  43,  44,  45,  45,  46,  47,  48,
   48,  49,  50,  50,  51,  52,  53,  53,  54,  55,  55,  56,  57,  58,  58,  59,
   60,  60,  61,  62,  62,  63,  64,  64,  65,  66,  66,  67,  68,  68,  69,  70,
   70,  71,  72,  72,  73,  74,  74,  75,  75,  76,  77,  77,  78,  79,  79,  80,
   80,  81,  82,  82,  83,  83,  84,  84,  85,  86,  86,  87,  87,  88,  88,  89,
   90,  90,  91,  91,  92,  92,  93,  93,  94,  94,  95,  95,  96,  96,  97,  97,
   98,  98,  99,  99, 100, 100, 101, 101, 102, 102, 103, 103, 104, 104, 104, 105,
  105, 106, 106, 107, 107, 107, 108, 108, 109, 109, 109, 110, 110, 111, 111, 111,
  112, 112, 112, 113, 113, 114, 114, 114, 115, 115, 115, 116, 116, 116, 116, 117,
  117, 117, 118, 118, 118, 118, 119, 119, 119, 120, 120, 120, 120, 121, 121, 121,
  121, 121, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 124, 124, 124, 124,
  124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126,
  126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127,
};

/**
 * \brief Prepare a typical sin_gen_request_t
 *
 * There is a bunch of cruft in a sin_gen_request_t struct that
 * requires thought to fill in properly.  This helper function makes a
 * lot of that easier to work with.
 *
 * Note that it will only return SIN_GEN_OKAY or SIN_GEN_INVALID, and
 * does not do full validation like sin_gen_generate does.
 *
 * Also note that it *does* fill out invalid requests (with the
 * exception of a NULL req pointer)!  It assumes that you know what
 * you're doing, and does what you ask.
 */
sin_gen_result_t sin_gen_populate(sin_gen_request_t *req,
			      uint8_t *buf,
			      uint16_t buflen,
			      uint32_t f_tone,
			      uint32_t f_sample) {
  if (req == NULL)
    return SIN_GEN_INVALID;

  memset(req, 0, sizeof(sin_gen_request_t));

  req->buf = buf;
  req->buflen = buflen;
  req->theta0 = SIN_THETA0;
  req->f_tone = f_tone;
  req->f_sample = f_sample;

  if ((buf == NULL) || (buflen == 0) || (f_tone == 0) || (f_sample == 0))
    return SIN_GEN_INVALID;

  return SIN_GEN_OKAY;
}

/**
 * \brief Get the appropriate entry from the sin table for the given angle
 *
 * \param theta Angle (radians) to get sin of
 */
uint8_t sin_gen_sin(float theta) {
  // cursor_pos = theta * 4*table_length/(2*pi) = theta*table_len/(pi/2)
  float cursor_pos_f = theta * SINE_TABLE_LENGTH/COS_THETA0;
  int ipart = cursor_pos_f;

  // Round up if needed: this bites us surprisingly often
  if (cursor_pos_f - ipart > 0.5) ipart++;

  // We should have to force this into the range [0, UINT16_MAX] here,
  // as float can go a long way off in either direction.  However,
  // because we ensured that our table length is a factor of
  // UINT16_MAX, we can just blindly cast here, and it will only throw
  // out bits that are equal to 0 modulo our table length.

  // Get down into our table space
  uint16_t cursor_pos = ipart % (4*SINE_TABLE_LENGTH);

  uint8_t cursor_quadrant = cursor_pos / SINE_TABLE_LENGTH;
  uint8_t i = cursor_pos % SINE_TABLE_LENGTH;
  if (cursor_quadrant & 1) i = SINE_TABLE_LENGTH - 1 - i;

  if (cursor_quadrant > 1)
    return 127-sin_table[i];
  return 127+sin_table[i];
}

/**
 * \brief Request the generation of a single sine cycle
 *
 * \param[IN,OUT] req The request, which also contains the result
 *
 * The input request has a lot of parameters.  The first entries are
 * straightforward: buf and buflen are a pointer to the buffer you'd
 * like the sine wave stored in, and how much space we may use in that
 * buffer.  Note that this function doesn't always fill the buffer:
 * its goal is to put a single sine wave into the buffer, and no more.
 * It returns the actual length of your output wave in the result_len
 * member of the request.  This is the period you should use in any
 * DMA transfer setups.
 *
 * The next parameter is the initial offset angle.  You probably don't
 * need to change this, but it seemed like a good idea to have it
 * available.  In the off chance that you're trying to DMA out I/Q
 * data, you would want to set this to COS_THETA0, to get the
 * quadrature signal on a second channel.
 *
 * Then come the two frequencies: f_tone is the frequency you want to
 * generate, and f_sample is how often the value being sent out the
 * DAC is updated (ie the clocking rate of the DMA channel driving the
 * DAC).  Your tone frequency must be less than half the sampling
 * frequency, or things won't work out (fancy name here is Nyquist
 * limit, but put another way: a sine wave needs to go up and then go
 * down every cycle.  That takes two samples, so you can't go faster
 * than half the sample rate).  If you try to violate the Nyquist
 * limit, you will get SIN_GEN_UNDERSAMPLED.
 *
 * Relatedly, if you ask for a very low frequency, it may be that a
 * single sine wave won't fit in your buffer.  If you're outputting at
 * 8kHz, a 1Hz sine wave will need 8,000 samples to complete.  If you
 * were to pass in a 1024 sample buffer in that case, it simply
 * wouldn't fit.  sin_gen_generate() will return SIN_GEN_TOO_SHORT in
 * this case, but it does populate as much of the sine wave as it can.
 * We just assume that you have some good reason for doing this.
 * Please see below for more about the phase error term, which will be
 * very important if you get SIN_GEN_TOO_SHORT.
 *
 * The final two members of the struct are return state from this
 * function.  The first, result_len, tells you how many samples your
 * sine wave takes up.  This is what you should use in anything that
 * consumes the sine wave (ie: the memory size in your DMA
 * peripheral).  The other return value is phase_error.  This is the
 * phase error at the last sample.  If your tone frequency isn't
 * divisible by the sampling frequency, there will be a little bit of
 * the sine wave cut off in the last sample.  This variable tells you
 * how much of a cutoff that is (in radians).  In particular, in a
 * SIN_GEN_TOO_SHORT case, there may be most of the waveform cut off.
 * If your application is sensitive to weird little bumps in a
 * perpetual sine wave, this variable will let you assess whether or
 * not a given configuration is acceptable.
 *
 * \returns A sin_gen_result_t of any errors encountered, SIN_GEN_OKAY on success
 *
 */
sin_gen_result_t sin_gen_generate(sin_gen_request_t *req) {
  float samples_per_wave;
  float theta;
  float dtheta; // radians per sample to generate our tone

  if (req == NULL) {
    return SIN_GEN_INVALID;
  }

  if ((req->buf == NULL) || (req->buflen == 0)) {
    return SIN_GEN_INVALID;
  }

  if ((req->f_tone == 0) || (req->f_sample == 0)) {
    return SIN_GEN_INVALID;
  }

  if (req->f_tone >= req->f_sample/2) {
    return SIN_GEN_UNDERSAMPLED;
  }

  // Compute how long this buffer needs to be.
  samples_per_wave = req->f_sample / req->f_tone;

  req->result_len = (uint16_t)samples_per_wave;
  if (samples_per_wave > req->buflen) {
    req->result_len = req->buflen;
  }

  req->phase_error = COS_THETA0*4 * (1 - (float)req->result_len/samples_per_wave);

  // \todo Evaluate extending by one sample if that reduces phase error
  //
  // If we have one more sample free before the end of the buffer, it
  // may result in less phase error to extend the buffer one cycle and
  // fill in there.


  // Now actually fill in the table
  theta = req->theta0;
  dtheta = 4*COS_THETA0 / samples_per_wave;

  for (int i = 0; i < req->result_len; i++) {
    req->buf[i] = sin_gen_sin(theta);
    theta += dtheta;
  }

  // After filling everything out, check if we should return TOO_SHORT
  if (samples_per_wave > req->buflen) {
    req->result_len = req->buflen;
    return SIN_GEN_TOO_SHORT;
  }


  return SIN_GEN_OKAY;
}


/** \} */ // End doxygen group
