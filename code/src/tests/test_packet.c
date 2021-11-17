#include <stdint.h>
#include <ctype.h>

#include "unity.h"

#include "packet.h"

//////////////////////////////////////////////////////////////////////
// Globals to pass state around

uint32_t G_frames_parsed;
uint8_t G_buf[1024];

uint8_t *G_rx_buf;
uint16_t G_rx_buflen;
uint8_t G_rx_addr;
uint8_t G_rx_control;
uint8_t G_rx_fcs_match;

uint8_t G_too_long_count;
uint8_t G_pkt_interrupted_count;

//////////////////////////////////////////////////////////////////////
// Utility functions

/**
 * Dump a buffer to stdout, because debugging escapes is hard.
 */
static void dump_buf(char *nom, uint8_t *buf, uint16_t buflen) {
  for (int k = 0; k < buflen; k++) {
    if (0 == (k%16)) printf("\n\t%s: ", nom);
    printf("%02x(%c) ", buf[k], (isalnum(buf[k]) ? buf[k] : '_'));
  }
  printf("\n");
}


//////////////////////////////////////////////////////////////////////
// Callbacks

static void parse_cb(uint8_t *buf, uint16_t buflen, uint8_t addr, uint8_t control, uint8_t fcs_match) {
  G_frames_parsed++;

  printf("\nGot packet: %d! addr=%02x control=%02x fcs_match=%d buf=%p\n",
         buflen, addr, control, fcs_match, buf);
  G_rx_buf = buf;
  G_rx_buflen = buflen;
  G_rx_addr = addr;
  G_rx_control = control;
  G_rx_fcs_match = fcs_match;

  //  dump_buf("G_rx_buf", G_rx_buf, buflen);
  printf("\n");
}

static void too_long_cb(uint8_t *buf, uint16_t buflen, uint8_t addr, uint8_t control, uint8_t fcs_match) {
  G_too_long_count++;
}

static void pkt_interrupted_cb(uint8_t *buf, uint16_t buflen, uint8_t addr, uint8_t control, uint8_t fcs_match) {
  G_pkt_interrupted_count++;
}


//////////////////////////////////////////////////////////////////////
// Unity requires a setUp and tearDown function.

void reset_globals(void) {
  printf("\nGlobal reset\n");
  G_rx_buf = NULL;
  G_rx_buflen = 0xbeef;
  G_rx_addr = '*';
  G_rx_control = '$';
  G_rx_fcs_match = 0;

  G_too_long_count = 0;
  G_pkt_interrupted_count = 0;
  G_frames_parsed = 0;
}

void setUp(void) {
  reset_globals();
  parser_setup(parse_cb, G_buf, 1024);
  parser_register_too_long_cb(too_long_cb);
  parser_register_pkt_interrupted_cb(pkt_interrupted_cb);
}

/**
 * There is nothing to tear down in this set of tests.
 */
void tearDown(void) {
}



//////////////////////////////////////////////////////////////////////
// The test case implementations

/**
 * Test that our initial state matches expectations.  Most of the test
 * cases call this to ensure that they are starting in the correct
 * state.
 */
void test_empty_string(void) {
  uint16_t expected = PACKET_FCS_INITIAL;
  uint16_t got = packet_fcs("", 0, PACKET_FCS_INITIAL);

  TEST_ASSERT_EQUAL(expected, got);
}

struct string_case {
  char *buf;
  uint16_t len;
  uint16_t expected;
};

/**
 * Test a bunch of strings
 */
void test_string_sets(void) {
#define NCASES 16
  struct string_case cases[NCASES] = {

                                      { "", 0, 65535 },
                                      { "0", 1, 15876 },
                                      { "00", 2, 30617 },
                                      { "000", 3, 14524 },
                                      { "0000", 4, 20060 },
                                      { "00000", 5, 43300 },
                                      { "000000", 6, 22028 },
                                      { "0000000", 7, 64441 },
                                      { "00000000", 8, 6450 },
                                      { "000000000", 9, 8971 },
                                      { "0000000000", 10, 36723 },
                                      { "00000000000", 11, 28688 },
                                      { "000000000000", 12, 8562 },
                                      { "0000000000000", 13, 24887 },
                                      { "00000000000000", 14, 29918 },
                                      { "000000000000000", 15, 3588 },
  };

  int i;

  for (i = 0; i < NCASES; i++) {
    uint16_t got = packet_fcs(cases[i].buf, cases[i].len, PACKET_FCS_INITIAL);
    TEST_ASSERT_EQUAL(cases[i].expected, got);
  }
}

struct framing_case {
  uint8_t *buf;
  uint16_t len;
  uint8_t *expected;
  uint16_t expected_len;
  uint8_t expected_pkt_interrupts;
};

void test_packet_framing() {
#undef NCASES
#define NCASES 2
  struct framing_case cases [NCASES] = {
                                        { "", 0, "~d\x0\x0\x0\xe8)~", 8, 0 },
                                        { "~asdf~foo}{}", 12, "~d\x00\x00\x10}~asdf}~foo}}{}}T\xc6~", 24, 0 },
  };

  uint8_t buf[1024];

  for (int i = 0; i < NCASES; i++) {
    char caseno[32];
    snprintf(caseno, 32, "Framing case %d", i);

    uint16_t got_len;
    memset(buf, '!', 1024);  // Make it easier to track what's going on
    got_len = packet_frame(buf, cases[i].buf, cases[i].len, 'd', 0);

    dump_buf(caseno, buf, got_len+8);

    TEST_ASSERT_EQUAL_MESSAGE(cases[i].expected_len, got_len, caseno);
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(cases[i].expected, buf, cases[i].len + 8, caseno);
  }
};


void test_packet_parsing() {
#undef NCASES
#define NCASES 3
  struct framing_case cases [NCASES] = {
                                        { "~asdf~foo}{}", 12, "~d\x00\x00\x10}~asdf}~foo}}{}}T\xc6~", 24, 0 },
                                        { "", 0, "~d\x0\x0\x0\xe8)~", 8, 0 },

                                        // Test a case where we get an unescaped ~ in body: should reset state
                                        { "~asdf~foo}{}", 12, "~d\x00\x00\x10}~a~d\x00\x00\x10}~asdf}~foo}}{}}T\xc6~", 32, 1 },
  };

  for (int i = 0; i < NCASES; i++) {
    char caseno[32];
    snprintf(caseno, 32, "Pars %d", i);

    reset_globals();
    memset(G_buf, '!', 1024);

    for (int j = 0; j < cases[i].expected_len; j++) {
      uint8_t x = cases[i].expected[j];
      packet_rx_byte(x);

      printf("%s %3d] %02x -> %14s: ", caseno, j, x, parser_state_name());
      dump_buf(caseno, G_buf, cases[i].expected_len);
    }
    TEST_ASSERT_EQUAL_MESSAGE(1, G_frames_parsed, caseno);
    TEST_ASSERT_EQUAL_MESSAGE(G_buf+1+1+1+2, G_rx_buf, caseno); // We expect that our rx buf is the buf we passed in
    TEST_ASSERT_EQUAL_MESSAGE(cases[i].len, G_rx_buflen, caseno);
    TEST_ASSERT_EQUAL_MESSAGE(0, G_rx_control, caseno);
    TEST_ASSERT_EQUAL_MESSAGE('d', G_rx_addr, caseno);
    TEST_ASSERT_EQUAL_MESSAGE(1, G_rx_fcs_match, caseno);
    TEST_ASSERT_EQUAL_MESSAGE(cases[i].expected_pkt_interrupts, G_pkt_interrupted_count, caseno);
    if (G_rx_buflen) {
      printf("Case buf: '%s' Got: '%s' (%p)\n", cases[i].buf, G_rx_buf, G_rx_buf);
      TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(cases[i].buf, G_rx_buf, cases[i].len, caseno);
    }
  }
}

struct rt_case {
  char *buf;
  uint16_t buflen;
  uint8_t addr;
  uint8_t cmd;
};

void test_packet_roundtrip() {
  struct rt_case cases[] = {
                            {"Hi mom", 0, 'C', 0},
                            {"~~", 0, '~', '~'},
                            {"", 0, 'C', 0},
                            {NULL, 0, 0, 0}
  };

  struct rt_case *cur_case = cases;
  int i = 0;
  uint8_t buf[1024];


  while (cur_case->buf != NULL) {
    char caseno[32];
    snprintf(caseno, 32, "RT case %d", i);

    // Reset state
    reset_globals();
    memset(buf, '!', 1024);
    memset(G_buf, '!', 1024);


    uint16_t buflen = cur_case->buflen;
    if (0 == buflen && cur_case->buf) {
      buflen = strlen(cur_case->buf);
    }

    dump_buf("TX msg", cur_case->buf, buflen);

    uint16_t got_len;

    got_len = packet_frame(buf, cur_case->buf, buflen,
                           cur_case->addr, cur_case->cmd);

    dump_buf("TX buf", buf, got_len);

    for (int j = 0; j < got_len; j++) {
      packet_rx_byte(buf[j]);
      printf("%s %3d] %02x -> %14s: ", caseno, j, buf[j], parser_state_name());
      dump_buf(caseno, G_buf, buflen+5);
    }

    TEST_ASSERT_EQUAL_MESSAGE(1, G_frames_parsed, caseno);
    TEST_ASSERT_EQUAL_MESSAGE(cur_case->addr, G_rx_addr, caseno);
    TEST_ASSERT_EQUAL_MESSAGE(cur_case->cmd, G_rx_control, caseno);
    TEST_ASSERT_EQUAL_MESSAGE(buflen, G_rx_buflen, caseno);
    TEST_ASSERT_EQUAL_MESSAGE(0, G_pkt_interrupted_count, caseno);
    TEST_ASSERT_EQUAL_MESSAGE(0, G_too_long_count, caseno);
    if (G_rx_buflen) { // precondition: must match buflen
      TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(cur_case->buf, G_rx_buf,
                                            buflen, caseno);
    }


    // And advance to next case
    i++; // advance counter
    cur_case++; // advance cursor
  }
}

//////////////////////////////////////////////////////////////////////
// Actual test runner

int main(int argc, char *argv[]) {
  UNITY_BEGIN();
  RUN_TEST(test_empty_string);
  RUN_TEST(test_string_sets);

  RUN_TEST(test_packet_framing);

  RUN_TEST(test_packet_parsing);

  RUN_TEST(test_packet_roundtrip);

  return UNITY_END();
}
