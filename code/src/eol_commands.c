#include "eol_commands.h"

#include <stdarg.h>
#include <stdio.h>

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
 * - Echo
 *
 * - Reset device 
 *
 * - Disable logline output for anything but errors or forced
 *
 * - DAC setup: prescaler, period, amplitude, number of sine points, number of waves
 *
 * - DAC start: begin the DAC DMA output
 *
 * - DAC stop: turn off the DAC
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

static uint32_t get32(uint8_t *c) {
  return (*c<<24)+(*(c+1)<<16)+(*(c+2)<<8)+(*(c+3));

}

static uint16_t get16(uint8_t *c) {
  return (*c<<8)+(*(c+1));

}

#define XMITBUFLEN 256
static uint8_t xmitbuf[XMITBUFLEN];

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

static void xmit_unk(uint8_t family, uint8_t subtype) {
  xmit_error(family, subtype, "Unknown family/subtype");
}

static uint8_t eol_dac_buf[1024];


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
    if ('Q' != subtype) {
      xmit_unk(family, subtype);
      return;
    }
    payload[1] = 'R';
    packet_send(payload, payload_len, addr, control);
    return;

  case 'L': //////////////////////////////////////// // Logging control
    if ('S' == subtype) {
      return;
    }
    xmit_unk(family, subtype);
    break;

  case 'R':  //////////////////////////////////////// // Reset
    if ('Q' != subtype) {
      xmit_unk(family, subtype);
      return;
    }
    scb_reset_system();
    // This busy-loops until the device resets
    break; // But to make linters happy

  case 'D': //////////////////////////////////////// // DAC
    if ('C' == subtype) {
      // DAC Configure:
      // uint16_t prescaler: as in dac_setup()
      // uint32_t period: as in dac_setup()
      // uint8_t scale: divisor for amplitude of sine wave as in sin_gen_sin()
      // uint16_t points_per_wave: Number of points in each sine wave
      // uint8_t num_waves: Number of waves to generate in the buffer
      uint16_t prescaler =       get16(cursor); cursor += 2;
      uint32_t period =          get32(cursor); cursor += 4;
      uint8_t scale =                  *cursor; cursor++;
      uint16_t points_per_wave = get16(cursor); cursor += 2;
      uint8_t num_waves =              *cursor; cursor++;

      sin_gen_request_t req;
      sin_gen_result_t res;

      uint16_t npts = points_per_wave * num_waves;

      console_dumps("DC %d %d %d %d %d", prescaler, period, scale, points_per_wave, num_waves);
      
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
      req.scale = 2;

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
      dac_start();
      xmit_ack(family, 's', "");
      return;
    }

    if ('T' == subtype) {
      dac_stop();
      xmit_ack(family, 't', "");
      return;
    }

    xmit_unk(family, subtype);
    break;

  default:
    xmit_unk(family, subtype);
    break;
    
    
  }
}

// buf,buflen -> actions


/** \} */ // End doxygen group
