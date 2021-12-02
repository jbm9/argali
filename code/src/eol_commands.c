#include "eol_commands.h"

#include <stdarg.h>
#include <stdio.h>

#include "console.h"
#include "sin_gen.h"
#include "dtmf.h"
#include "packet.h"

#ifndef TEST_UNITY
#include "dac.h"
#include "adc.h"

#include <libopencm3/cm3/scb.h>
#else
#include "hardware_dummy.c"
#endif

/**
 * \file eol_commands.c
 * \brief EOL Commands Implementation
 */

/**
 * \defgroup eol_commands EOL Commands
 * \{
 *
 * A set of commands for poking and prodding at TamoDevBoard on the
 * EOL stand.
 *
 * This implements a fast-and-loose protocol for submitting requests
 * for DAC output, ADC input, etc.
 *
 * Note that these do not lock out normal functionality, so human
 * interaction with the board under test may upset results.  We may
 * wish to update our drivers to support this kind of shimming better
 * in the future.
 *
 * Commands to support:
 *
 * - x Echo
 *
 * - x Reset device
 *
 * - Disable logline output for anything but errors or forced
 *
 * - x DAC setup: prescaler, period, amplitude, number of sine points, number of waves
 *
 * - x DAC start: begin the DAC DMA output
 *
 * - x DAC stop: turn off the DAC
 *
 * - ADC Setup: prescaler, period, channels, buffer length
 *
 * - ADC run: run the ADC input, and dump results after one buffer, then stop ADC
 *
 * - Goertzel setup: num of filters, coefficients for each filter
 *
 * - Goertzel run: N (With a set up ADC and Goertzel, run N buffers and dump results)

 *
 * Command format:
 *
 * [uint8_t family] [uint8_t subtype] [variable length args]
 *
 * We'll just unpack things pretty directly here
 */

////////////////////////////////////////////////////////////
// State variables

static uint8_t eol_dac_buf[1024];

static const uint16_t eol_adc_buf_len = 2048;
static uint8_t eol_adc_buf[2048];


#define XMITBUFLEN 1024
static uint8_t xmitbuf[XMITBUFLEN];


////////////////////////////////////////////////////////////
// Utility functions

static uint32_t get32(uint8_t *c) {
  return (*c<<24)+(*(c+1)<<16)+(*(c+2)<<8)+(*(c+3));

}

static uint16_t get16(uint8_t *c) {
  return (*c<<8)+(*(c+1));

}


static void xmit_ack(uint8_t family, uint8_t subtype, char *fmt, ...) {
  uint16_t buflen;

  // We'll vsnprintf() into our buffer a bit, then slip the constants
  // in by hand later on.

  va_list argp;
  va_start(argp, fmt);
  buflen = vsnprintf((char*)xmitbuf+2, XMITBUFLEN-2, fmt, argp);
  va_end(argp);

  // And stash in our payload header
  xmitbuf[0] = family;
  xmitbuf[1] = subtype;

  packet_send(xmitbuf, buflen+3, 'E', '!');
}

static void xmit_error(uint8_t family, uint8_t subtype, char *fmt, ...) {
  uint16_t buflen;

  // We'll vsnprintf() into our buffer a bit, then slip the constants
  // in by hand later on.

  va_list argp;
  va_start(argp, fmt);
  buflen = vsnprintf((char*)xmitbuf+3, XMITBUFLEN-3, "%s", argp);
  va_end(argp);

  // And stash in our payload header
  xmitbuf[0] = '!';
  xmitbuf[1] = family;
  xmitbuf[2] = subtype;

  packet_send(xmitbuf, buflen+3, 'E', '!');
}

static void xmit_buf(uint8_t family, uint8_t subtype, const uint8_t *buf, uint16_t buflen) {
  xmitbuf[0] = family;
  xmitbuf[1] = subtype;
  memcpy(xmitbuf+2, buf, buflen);
  packet_send(xmitbuf, buflen+2, 'E', 'B');
}

static void xmit_unk(uint8_t family, uint8_t subtype) {
  xmit_error(family, subtype, "Unknown family/subtype");
}

////////////////////////////////////////////////////////////////////////////////
// Callbacks

static void eol_adc_callback(const uint8_t *buf, uint16_t buflen) {
  uint16_t len_to_send;
  adc_stop();

  const uint16_t stride = XMITBUFLEN / 4;

  for (const uint8_t *c = buf; c < buf+buflen; c += stride) {
    len_to_send = stride;
    if (buflen - (c-buf) < stride)
      len_to_send = buflen - (c-buf);
    xmit_buf('A', 'C', c, len_to_send);

  }
}


////////////////////////////////////////////////////////////
// Actual implementation goes here.

void eol_command_handle(uint8_t *payload, uint16_t payload_len,
                        uint8_t addr, uint8_t control,
                        uint8_t fcs_match) {
  if (!fcs_match) {
    xmit_error('?','?', "FCS mismatch, addr=%02x control=%02x len=%d",
               addr, control, payload_len);
    return;
  }

  // We use this cursor all over the place, so make it shared
  uint8_t *cursor = payload+2;

  uint8_t family = payload[0];
  uint8_t subtype = payload[1];

  switch(family) {
  case 'E': //////////////////////////////////////// // Echo
    if ('Q' == subtype) { // Query
      payload[1] = 'R';
      packet_send(payload, payload_len, addr, control);
      return;
    }

    if ('T' == subtype) { // full byte table request
      // This is available to sanity check the link and its encoding.
      eol_dac_buf[0] = 0;
      for (uint8_t i = 1; i != 0; i++) {
        eol_dac_buf[i] = i;
      }
      xmit_buf('E', 'U', eol_dac_buf, 256);
      return;
    }

    xmit_unk(family, subtype);
    return;

  case 'L': //////////////////////////////////////// // Logging control
    if ('S' == subtype) {
      return;
    }

    xmit_unk(family, subtype);
    return;

  case 'R':  //////////////////////////////////////// // Reset
    if ('Q' == subtype) {
      scb_reset_system();     // This busy-loops until the device resets
      return; // But make linters happy
    }

    xmit_unk(family, subtype);
    return; // But to make linters happy

  case 'D': //////////////////////////////////////// // DAC
    if ('C' == subtype) {
      // DAC Configure:
      // uint16_t prescaler: as in dac_setup()
      // uint32_t period: as in dac_setup()
      // uint8_t scale: divisor for amplitude of sine wave as in sin_gen_sin()
      // uint16_t points_per_wave: Number of points in each sine wave
      // uint8_t num_waves: Number of waves to generate in the buffer
      // uint8_t theta0_u8: Offset of theta0, in 1/256 of a wave steps
      uint16_t prescaler =       get16(cursor); cursor += 2;
      uint32_t period =          get32(cursor); cursor += 4;
      uint8_t scale =                  *cursor; cursor++;
      uint16_t points_per_wave = get16(cursor); cursor += 2;
      uint8_t num_waves =              *cursor; cursor++;
      uint8_t theta0_u8 =              *cursor; cursor++;

      sin_gen_request_t req;
      sin_gen_result_t res;

      uint16_t npts = points_per_wave * num_waves;

      console_dumps("DC %d %d %d %d %d %d\n", prescaler, period, scale, points_per_wave, num_waves, theta0_u8);

      //////////////////////////////////////////////////
      // Fill our sine buffer
      //
      // To get the desired behavior, we fib to the sine generator and
      // tell it that we want 1 Hz at the number of points per wave.
      // Then, to set ourselves up for num_waves of waves, we set the
      // buffer to be exactly that many points long.  Finally, we use
      // the sin_gen_generate_fill(), which blindly stuffs as much as
      // it can into the buffer.  We just happen to have set things up
      // such that it can do its job perfectly.
      res = sin_gen_populate(&req, eol_dac_buf, npts, 1, points_per_wave);
      if (SIN_GEN_OKAY != res) {
        xmit_error(family, subtype,
                   "Failed to populate sin_gen request, bailing on DAC setup: %s!",
                   sin_gen_result_name(res));
        return;
      }
      req.scale = scale;
      req.theta0 = 4*COS_THETA0/256 * theta0_u8;

      res = sin_gen_generate_fill(&req);
      if (SIN_GEN_OKAY != res) {
        xmit_error(family, subtype,
                   "Failed to fill sin_gen request, bailing on DAC setup: %s!",
                   sin_gen_result_name(res));
        return;
      }

      ////////////////////////////////////////
      // Set up DAC DMA at this buffer
      dac_stop();
      dac_setup(prescaler, period, eol_dac_buf, npts);
      xmit_ack(family, 'c', "DAC configured: %dHz", (int)dac_get_sample_rate(prescaler, period));
      return;
    } // DAC Configure

    if ('S' == subtype) {
      // DAC Start:
      // No parameters
      dac_start();
      xmit_ack(family, 's', "");
      return;
    }

    if ('T' == subtype) {
      // DAC sTop:
      // No parameters
      dac_stop();
      xmit_ack(family, 't', "");
      return;
    }

    xmit_unk(family, subtype);
    return;

  case 'A': //////////////////////////////////////// // ADC
    if ('C' == subtype) {
      // ADC Capture
      // uint16_t prescaler: as in dac_setup()
      // uint32_t period: as in dac_setup()
      // uint16_t num_points: Number of samples to capture
      // uint8_t sample_width: 1 for 8b, 2 for 16b samples
      // uint8_t num_channels: number of channels to capture
      // uint8_t[] channels: list of channels to capture
      //
      // Note that this returns num_channels*num_points samples,
      // interleaved just as the ADC DMA gives it to us.  These are
      // all sample_width bytes wide, for the final size of your buffer.
      //
      // Also, note that we don't autoconfigure the channels as inputs
      // here.  You actually only have access to 0 and 1 in the
      // default state.d
      uint16_t prescaler =       get16(cursor); cursor += 2;
      uint32_t period =          get32(cursor); cursor += 4;
      uint16_t num_points =      get16(cursor); cursor += 2;
      uint8_t sample_width =           *cursor; cursor++;
      uint8_t num_channels =           *cursor; cursor++;

      uint16_t buflen = num_points * sample_width * num_channels;

      if (buflen > eol_adc_buf_len) {
        xmit_error('A', 'C', "Buffer truncation! %d bytes available, %d requested", buflen, eol_adc_buf_len);
        return;
      }

      adc_config_t adc_config = {
                                 .prescaler = prescaler,
                                 .period = period,
                                 .buf = eol_adc_buf,
                                 .buflen = buflen,
                                 .double_buffer = 0,
                                 .n_channels = num_channels,
                                 .sample_width = sample_width,
                                 .cb = eol_adc_callback,
      };

      console_dumps("AC ps=%d pd=%d np=%d sw=%d nc=%d bl=%d\n",
                    prescaler, period, num_points, sample_width, num_channels, buflen);

      for (int i = 0; i < num_channels; i++) {
        adc_config.channels[i] = *cursor; cursor++;
      }

      adc_setup(&adc_config);
      adc_start();
    }
    // No ACK here, the ADC data packets will cover that for us
    return;

  default:
    xmit_unk(family, subtype);
    return;
  }
}

// buf,buflen -> actions


/** \} */ // End doxygen group
