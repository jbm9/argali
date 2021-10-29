// A quick note: getting this to work in unity required bodging the
// unity_tests.mk Makefile.  The key part was adding -lm to the end of
// the linker call:
//
// $(PATHB)test_%.$(TARGET_EXTENSION):  $(PATHO)test_%.o $(PATHO)%.o $(PATHU)unity.o #$(PATHD)test_%.d
//	$(LINK) -o $@ $^ -lm
// # Note janky addition of -lm above for the DTMF tests
//
// Including this in here for posterity, in case this ever gets
// separated from the janky Makefile.

#include <stdint.h>
#include <string.h>

#include "unity.h"

#include "dtmf.h"

#include "fixture_dtmf_tones.c"

//! Our test dummy

#define DUMMY_LEN 256
uint8_t rx_down[DUMMY_LEN];
uint8_t rx_down_cursor = 0;

uint8_t rx_up[DUMMY_LEN];
uint8_t rx_up_cursor = 0;



// Fixture for our table

//////////////////////////////////////////////////////////////////////
// Unity requires a setUp and tearDown function.

/**
 * Blank out our request
 */
void setUp(void) {
  rx_down_cursor = 0;
  rx_up_cursor = 0;

  memset(rx_down, 'd', DUMMY_LEN);
  memset(rx_up, 'u', DUMMY_LEN);

  // Set NULs at beginning
  rx_down[0] = 0;
  rx_up[0] = 0;
}

/**
 * There is nothing to tear down in this set of tests.
 */
void tearDown(void) {}

//////////////////////////////////////////////////////////////////////
// Helper macros


void button_down(uint8_t symbol) {
  printf("Got button down! %x\n", symbol);
  rx_down[rx_down_cursor] = symbol;
  rx_down_cursor++;
  rx_down[rx_down_cursor] = 0; // Make sure we have a NUL
}

void button_up(uint8_t symbol, float dt) {
  printf("Got button up! %x: %0.4ff\n", symbol, dt);
  rx_up[rx_up_cursor] = symbol;
  rx_up_cursor++;
  rx_up[rx_up_cursor] = 0; // Make sure we have a NUL
}


//////////////////////////////////////////////////////////////////////
// The test case implementations

void test_happy_path(void) {
  const int buf_stride = 200;

  dtmf_init(FIXTURE_DTMF_FS, 0.2, button_down, button_up);

  for (int i = 0; i < FIXTURE_DTMF_BUFLEN; i+= buf_stride) {
    uint16_t l = FIXTURE_DTMF_BUFLEN - i;
    l = (l > buf_stride ? buf_stride : l);

    dtmf_process(fixture_dtmf_buffer+i, l);
  }
  dtmf_process(NULL, 0);

  TEST_ASSERT_EQUAL_STRING(fixture_dtmf_symbols, rx_down);
  TEST_ASSERT_EQUAL_STRING(fixture_dtmf_symbols, rx_up);
}

void test_get_tones__invalid_inputs(void) {
  float r, c;

  dtmf_status_t s;

  s = dtmf_get_tones('0', NULL, &c);
  TEST_ASSERT_EQUAL(DTMF_INVALID_INPUTS, s);

  s = dtmf_get_tones('0', &r, NULL);
  TEST_ASSERT_EQUAL(DTMF_INVALID_INPUTS, s);

  s = dtmf_get_tones('0', NULL, NULL);
  TEST_ASSERT_EQUAL(DTMF_INVALID_INPUTS, s);

}

void test_get_tones__missing_symbol(void) {
  float r, c;

  dtmf_status_t s;

  s = dtmf_get_tones(0, &r, &c);
  TEST_ASSERT_EQUAL(DTMF_SYMBOL_NOT_FOUND, s);

  s = dtmf_get_tones(8, &r, &c);
  TEST_ASSERT_EQUAL(DTMF_SYMBOL_NOT_FOUND, s);

  s = dtmf_get_tones(0xff, &r, &c);
  TEST_ASSERT_EQUAL(DTMF_SYMBOL_NOT_FOUND, s);

  s = dtmf_get_tones('E', &r, &c);
  TEST_ASSERT_EQUAL(DTMF_SYMBOL_NOT_FOUND, s);

  s = dtmf_get_tones('a', &r, &c);
  TEST_ASSERT_EQUAL(DTMF_SYMBOL_NOT_FOUND, s);
}


void test_get_tones__happy_path(void) {
  float r, c;

  dtmf_status_t s;

  float rows[16] = { 697, 697, 697, 697,
		     770, 770, 770, 770,
		     852, 852, 852, 852,
		     941, 941, 941, 941, };
  float cols[16] = { 1209, 1336, 1477, 1633,
		     1209, 1336, 1477, 1633,
		     1209, 1336, 1477, 1633,
		     1209, 1336, 1477, 1633,
  };
  uint8_t syms[16] = "123A456B789C*0#D";

  for(int i = 0; i < 16; i++) {
    char errmsg[256];
    snprintf(errmsg, 255, " case %d ('%c')", i, syms[i]);

    s = dtmf_get_tones(syms[i], &r, &c);
    TEST_ASSERT_EQUAL_MESSAGE(DTMF_OKAY, s, errmsg);
    TEST_ASSERT_EQUAL_MESSAGE(rows[i], r, errmsg);
    TEST_ASSERT_EQUAL_MESSAGE(cols[i], c, errmsg);
  }

}

void test_all_zeros(void) {
  const int buf_stride = 200;

  // With all zeros as input, we expect no hits, even at very low
  // thresholds.  HOWEVER, we don't get that, because our zero is
  // actually 127.0.  This causes some consternation.

  uint8_t zeros[FIXTURE_DTMF_BUFLEN];
  memset(zeros, 0, FIXTURE_DTMF_BUFLEN);


  dtmf_init(FIXTURE_DTMF_FS, 0.4, button_down, button_up);

  for (int i = 0; i < FIXTURE_DTMF_BUFLEN; i+= buf_stride) {
    uint16_t l = FIXTURE_DTMF_BUFLEN - i;
    l = (l > buf_stride ? buf_stride : l);

    dtmf_process(zeros+i, l);
  }
  dtmf_process(NULL, 0);

  TEST_ASSERT_EQUAL_STRING("", rx_down);
  TEST_ASSERT_EQUAL_STRING("", rx_up);
}


//////////////////////////////
// dtmf tests

//////////////////////////////////////////////////////////////////////
// Actual test runner

int main(int argc, char *argv[]) {
  UNITY_BEGIN();

  RUN_TEST(test_happy_path);
  RUN_TEST(test_all_zeros);

  RUN_TEST(test_get_tones__invalid_inputs);
  RUN_TEST(test_get_tones__missing_symbol);
  RUN_TEST(test_get_tones__happy_path);



  return UNITY_END();
}
