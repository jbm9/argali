#include "packet.h"
#include "logging.h"

#ifdef TEST_UNITY
#warning On a test stand; building without packet_send
#else
#include "console.h"
#endif

/**
 * \file packet.c
 * \brief Packet framing implementation
 *
 * \defgroup packet A line protocol for serial comms, used for testing
 * \addtogroup packet
 * \{
 *
 * For testing, we need a very reliable framing that can be used to
 * manage the device.  It will need to be able to reset the
 * microcontroller out-of-band, and does not need to support raw human
 * typed input.
 *
 * It also needs to support meaningful checksums of each packet.
 *
 * We use a framing related to HDLC (by way of PPP).  This is supposed
 * to implement the same checksum as PPP, CRC16_CCITT_FALSE.  However,
 * I have not yet found good test cases for this, so it's not
 * necessarily fully correct.  That said, we have an implementation in
 * C and python, which suffices.
 *
 */

/**
 * \subsection Packet Framing Format
 *
 <table>
 <caption>Argali Packet Framing</caption>
 <tr><td>Flag</td><td>Address</td><td>Reserved</td><td>Length High</td></tr>
 <tr><td>Length Low</td><td colspan="3">Payload</td></tr>
 <tr><td colspan="4">Payload</td></tr>
 <tr><td colspan="4">Payload</td></tr>
 <tr><td>Payload</td><td colspan="2">FCS</td><td>Flag</td></tr>
 </table>
*/

/**
 * \subsection Packet parsing state machine
 *
 * \dot
 digraph G {
 IDLE -> WAIT_ADDR [label="got 0x7E"] ;
 IDLE -> IDLE [label="Got anything but 0x7E"];
 WAIT_ADDR -> WAIT_CONTROL [label = "Got byte"];
 WAIT_ADDR -> WAIT_ADDR [label = "Got 0x7E"];
 WAIT_CONTROL -> WAIT_LENGTH_HI [label="Got byte"];
 WAIT_LENGTH_HI -> WAIT_LENGTH_LO [label="Got byte"];
 WAIT_LENGTH_LO -> IN_BODY [label = "Length complete"];
 WAIT_LENGTH_LO -> IDLE [label = "Packet length exceeds maximum"];
 IN_BODY -> ESCAPE_FOUND [label = "got 0x7D"];
 ESCAPE_FOUND -> IN_BODY [label = "Resolve escaping"];
 IN_BODY -> IN_BODY [label="byte"];
 IN_BODY -> WAIT_CKSUM_HI;
 WAIT_CKSUM_HI -> WAIT_CKSUM_LO;
 WAIT_CKSUM_LO -> IDLE;
 }
 \enddot
*/


#ifdef TEST_UNITY
#warning On a test stand; building without packet_send
#else

/**
 * \brief Sends a single packet out over serial
 */
void packet_send(const uint8_t *buf, uint16_t buflen, uint8_t address, uint8_t command) {
  uint8_t pktbuf[1024];
  uint16_t pktlen;
  pktlen = packet_frame(pktbuf, (uint8_t*)buf, buflen, address, command);

  int i;
  for (i = 0; i < 4; i++) {
    console_send_blocking('~');
  }

  for (i = 0; i < pktlen; i++) {
    console_send_blocking(pktbuf[i]);
  }
  for (i = 0; i < 4; i++) {
    console_send_blocking('~');
  }
}
#endif


/**
 * \brief Creates a fully-escaped packet in dst for the given buffer.
 *
 * \param dst Destination buffer, must be adequately sized

 * \param buf Buffer containing the payload
 * \param buflen Length of the input buffer
 * \param address Target address
 * \param command Command to be used (but will never be)
 *
 */
uint16_t packet_frame(uint8_t *dst, const uint8_t *buf, uint16_t buflen, uint8_t address, uint8_t command) {
  uint8_t *c; // Cursor to copy over with escaping

  c = dst;
  *c = PACKET_FLAG; c++;
  *c = address; c++;
  *c = command; c++;
  // Don't write the length yet, so skip two bytes
  c += 2;

  for (int i = 0; i < buflen; i++) {
    if ( (buf[i] == PACKET_FLAG) || (buf[i] == PACKET_ESCAPE) ) {
      *c = PACKET_ESCAPE;
      c++;
    }
    *c = buf[i];
    c++;
  }

  // Write out the destination length
  uint16_t dstlen = c - dst - 1-1-1-2;  // length doesn't include header bytes
  dst[3] = (dstlen & 0xFF00) >> 8;
  dst[4] = (dstlen & 0xFF);

  uint16_t fcs = packet_fcs(dst+1, dstlen+1+1+2, PACKET_FCS_INITIAL);

  *c = (fcs & 0xFF00) >> 8; c++;
  *c = (fcs & 0xFF); c++;
  *c = PACKET_FLAG; c++;


  return dstlen + PACKET_FRAMING_OVERHEAD;
}


/**
 * \brief (INTERNAL) Run one step of the FCS algorithm
 *
 * \param x The byte to be checksummed
 * \param crc The existing state, before stepping forward
 *
 */
static uint16_t fcs_step(uint8_t x, uint16_t crc) {
  for (int j = 0; j < 8; j++) {
    crc=((crc&1)^(x&1))?(crc>>1)^0x8408:crc>>1; // crc calculator
    x >>= 1;
  }
  return crc;
}


/**
 * \brief Compute the FCS for the buffer, starting from state crc
 *
 * \param buf The data to checksum
 * \param buflen Length of data in bytes
 * \param crc The starting point for the CRC; use 0xFFFF for the first pass
 *
 * To use this iteratively, keep passing in the CRC received from the
 * last call
 */
uint16_t packet_fcs(const uint8_t *buf, uint16_t buflen, uint16_t crc) {
  int i;

  for (i = 0; i < buflen; i++) {
    uint8_t x = buf[i];
    crc = fcs_step(x, crc);
  }
  return crc;
}

static packet_parser_data_t parse_state;


static void parser_reset(void) {
  parse_state.state = IDLE;
  parse_state.bytes_rem = 0;
  parse_state.buf_cursor = 0;
  parse_state.fcs = PACKET_FCS_INITIAL;
}


void parser_setup(parser_callback cb, uint8_t *buf, uint16_t buflen) {
  parser_reset();
  parse_state.callback = cb;
  parse_state.rx_buf = buf;
  parse_state.rx_buf_len = buflen;
}

const char *parser_state_name() {
  switch(parse_state.state) {
  case IDLE: return "IDLE";
  case  WAIT_ADDR: return "WAIT_ADDR";
  case  WAIT_CONTROL: return "WAIT_CONTROL";
  case  WAIT_LENGTH_HI: return "WAIT_LENGTH_HI";
  case  WAIT_LENGTH_LO: return "WAIT_LENGTH_LO";
  case  IN_BODY: return "IN_BODY";
  case  ESCAPE_FOUND: return "ESCAPE_FOUND";
  case  WAIT_CKSUM_HI: return "WAIT_CKSUM_HI";
  case  WAIT_CKSUM_LO: return "WAIT_CKSUM_LO";
  default:
    return "???";

  }
}

void packet_rx_byte(uint8_t c) {
  int is_flag = (c == PACKET_FLAG);
  int is_escape = (c == PACKET_ESCAPE);

  // The link can idle by sending flags repeatedly, so we will just
  // quietly drop repeated entries but keep in the WAIT_ADDR state.

  if ((parse_state.state == WAIT_ADDR) && is_flag) {
    return;
  }

  // Ignore noise on the line while waiting for a flag
  if ((parse_state.state == IDLE) && !is_flag) {
    return;
  }

  // We copy over every byte, except for escapes or when the link is in WAIT_ADDR
  if (!((parse_state.state == IN_BODY) && is_escape)) {
    parse_state.rx_buf[parse_state.buf_cursor] = c;
    parse_state.buf_cursor++;

    // And we checksum everything from the address byte up until we
    // get to WAIT_CKSUM_HI.
    if ((parse_state.state != IDLE) &&
        (parse_state.state != WAIT_CKSUM_HI) &&
        (parse_state.state != WAIT_CKSUM_LO)) {
      parse_state.fcs = fcs_step(c, parse_state.fcs);
    }
  } else {
    // Got an escape, need to FCS that, too.
    parse_state.fcs = fcs_step(c, parse_state.fcs);
  }

  // If we're in the body (or got an escape in the body), expect one
  // less byte of input now.
  if ((parse_state.state == IN_BODY) || (parse_state.state == ESCAPE_FOUND)) {
    parse_state.bytes_rem--;
  }

  if ((parse_state.state == IN_BODY) && is_flag) {
    // An unescaped ~ in a body likely means a previous transfer was
    // abandoned, and we are now picking up a new one.  Reset to
    // IDLE, then fall through
    parser_reset();
  }

  switch(parse_state.state) {
  case IDLE:
    if (is_flag) {
      parse_state.state = WAIT_ADDR;
      parse_state.rx_buf[0] = '~';
      parse_state.buf_cursor = 1;
      parse_state.fcs = PACKET_FCS_INITIAL;
    }
    break;

  case WAIT_ADDR:
    parse_state.addr = c;
    parse_state.state = WAIT_CONTROL;
    break;

  case WAIT_CONTROL:
    parse_state.control = c;
    parse_state.state = WAIT_LENGTH_HI;
    break;

  case WAIT_LENGTH_HI:
    parse_state.pktlen = c<<8;
    parse_state.state = WAIT_LENGTH_LO;
    break;

  case WAIT_LENGTH_LO:
    parse_state.pktlen += c;

    parse_state.bytes_rem = parse_state.pktlen;
    if (parse_state.bytes_rem > PACKET_MAX_PAYLOAD_LENGTH) {
      // logline(LEVEL_ERROR, "Packet size too big: %d, aborting", parse_state.bytes_rem);
      parser_reset();
      return;
    }
    parse_state.state = (parse_state.pktlen ? IN_BODY : WAIT_CKSUM_HI);

    break;

  case IN_BODY:
    if (is_escape) {
      parse_state.state = ESCAPE_FOUND;
      break;
    }

    if (parse_state.bytes_rem == 0)
      parse_state.state = WAIT_CKSUM_HI;
    break;

  case ESCAPE_FOUND:
    parse_state.state = IN_BODY;

    if (parse_state.bytes_rem == 0)
      parse_state.state = WAIT_CKSUM_HI;

    break;

  case WAIT_CKSUM_HI:
    parse_state.fcs_expected = c<<8;
    parse_state.state = WAIT_CKSUM_LO;

    break;

  case WAIT_CKSUM_LO:
    parse_state.fcs_expected += c;

    uint8_t fcs_match = (parse_state.fcs_expected == parse_state.fcs);

    if (parse_state.callback) {
      parse_state.callback(parse_state.rx_buf+5,
                           parse_state.buf_cursor - PACKET_FRAMING_OVERHEAD + 1,
                           parse_state.addr,
                           parse_state.control,
                           fcs_match);
    }

    // Reset parser state
    parser_reset();
    break;

  default:
    break;
  }
}


/** \} */
