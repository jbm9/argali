#pragma once

#include <stdint.h>

/**
 * \file packet.h
 * \brief Header file for serial comms packetization
 *
 * \addtogroup packet
 * \{
 */

#include <stdint.h>
#include <string.h>

#define PACKET_FLAG 0x7E  //!< Flag used to begin packets
#define PACKET_ESCAPE 0x7D  //!< Escaping character for packets

#define PACKET_MAX_LENGTH 1024  //!< Maximum length of a packet

#define PACKET_FRAMING_OVERHEAD 8 //!< The number of bytes that are used for framing overhead

#define PACKET_MAX_PAYLOAD_LENGTH PACKET_MAX_LENGTH - PACKET_FRAMING_OVERHEAD //!< Maximum payload length

#define PACKET_FCS_INITIAL 0xFFFF  //!< Default initial state of our FCS checksum

uint16_t packet_fcs(const uint8_t *, uint16_t, uint16_t);
uint16_t packet_frame(uint8_t *, const uint8_t *, uint16_t, uint8_t, uint8_t);

/**
 * \brief The possible states our parser can be in
 */
enum parser_state {
                   IDLE = 0,       //!< Channel is idle, waiting for preamble
                   WAIT_ADDR,      //!< Got preamble, waiting for an address
                   WAIT_CONTROL,   //!< Got address, waiting for control word
                   WAIT_LENGTH_HI, //!< Got control word, waiting for first length byte
                   WAIT_LENGTH_LO, //!< Got first length byte, waiting for lower byte
                   IN_BODY,        //!< Receiving body data
                   WAIT_CKSUM_HI,  //!< Done with body, waiting for FCS first byte
                   WAIT_CKSUM_LO,  //!< Got first FCS byte, waiting for second to complete
};

/**
 * A callback for when a completed packet is received.
 *
 * First parameter a pointer to the completed buffer (with escapes still intact)
 * Second is the buffer length
 * Third is the address given
 * Fourth is the control code given
 * Final argument is whether or not the checksum matched
 */
typedef   void (*parser_callback)(uint8_t *, uint16_t, uint8_t, uint8_t, uint8_t);

/**
 * \brief State used in packet parsing
 */
typedef struct packet_parser_data {
  enum parser_state state;
  uint8_t saw_escape; //!< Unescaped escapes do not escape our first input.
  uint8_t *rx_buf;
  uint16_t rx_buf_len;
  uint16_t buf_cursor;

  uint16_t bytes_rem;

  uint8_t addr;
  uint8_t control;
  uint16_t pktlen;
  uint16_t fcs;
  uint16_t fcs_expected;

  parser_callback callback;
  parser_callback too_long_callback;
  parser_callback pkt_interrupted_callback;

} packet_parser_data_t;


const char *parser_state_name(void);
void parser_setup(parser_callback, uint8_t *, uint16_t);
void parser_register_too_long_cb(parser_callback);
void parser_register_pkt_interrupted_cb(parser_callback);
void packet_rx_byte(uint8_t);
void packet_send(const uint8_t *, uint16_t, uint8_t, uint8_t);
/** \} */
