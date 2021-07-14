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

#define PACKET_FCS_INITIAL 0xFFFF  //!< Default initial state of our FCS checksum

uint16_t packet_fcs(const uint8_t *, uint16_t, uint16_t);
uint16_t packet_frame(uint8_t *, const uint8_t *, uint16_t, uint8_t, uint8_t);

/**
 * \brief The possible states our parser can be in
 */
enum parser_state {
                   IDLE = 0,
                   WAIT_ADDR,
                   WAIT_CONTROL,
                   WAIT_LENGTH_HI,
                   WAIT_LENGTH_LO,
                   IN_BODY,
                   ESCAPE_FOUND,
                   WAIT_CKSUM_HI,
                   WAIT_CKSUM_LO,
};

typedef   void (*parser_callback)(uint8_t *, uint16_t, uint8_t, uint8_t, uint8_t);

/**
 * \brief State used in packet parsing
 */
typedef struct packet_parser_data {
  enum parser_state state;
  uint8_t *rx_buf;
  uint16_t buf_cursor;

  uint16_t bytes_rem;

  uint8_t addr;
  uint8_t control;
  uint16_t pktlen;
  uint16_t fcs;
  uint16_t fcs_expected;

  parser_callback callback;

} packet_parser_data_t;


const char *parser_state_name(void);
void parser_init(parser_callback, uint8_t *);
void packet_rx_byte(uint8_t);

/** \} */
