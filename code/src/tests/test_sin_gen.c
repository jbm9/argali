#include <stdint.h>
#include <string.h>

#include "unity.h"

#include "sin_gen.h"

//! Our test dummy
static sin_gen_request_t test_request_body;
static sin_gen_request_t *test_req = &test_request_body;
static uint8_t dummy_buf[1024];
static uint16_t dummy_buflen = 1024;


// Fixture for our table
static const uint8_t expected_sin_table[256] = {
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

//////////////////////////////////////////////////////////////////////
// Unity requires a setUp and tearDown function.

/**
 * Blank out our request
 */
void setUp(void) {
  memset(test_req, 0, sizeof(sin_gen_request_t));
}

/**
 * There is nothing to tear down in this set of tests.
 */
void tearDown(void) {}

//////////////////////////////////////////////////////////////////////
// Helper macros

#define TEST_ASSERT_SG_REQ_EQ(ebuf,ebuflen,etheta0,escale,ef_tone,ef_sample) { \
    TEST_ASSERT_EQUAL(ebuf, test_req->buf);				\
    TEST_ASSERT_EQUAL(ebuflen, test_req->buflen);			\
    TEST_ASSERT_FLOAT_WITHIN(1e-9, etheta0, test_req->theta0);		\
    TEST_ASSERT_EQUAL(escale, test_req->scale);				\
    TEST_ASSERT_EQUAL(ef_tone, test_req->f_tone);			\
    TEST_ASSERT_EQUAL(ef_sample, test_req->f_sample);			\
  }

#define TEST_ASSERT_SG_RES_EQ(eresult_len, ephase_error) {		\
    TEST_ASSERT_EQUAL(eresult_len, test_req->result_len);		\
    TEST_ASSERT_FLOAT_WITHIN(0.01, ephase_error, test_req->phase_error); \
  }


//////////////////////////////////////////////////////////////////////
// The test case implementations


//////////////////////////////
// sin_gen_populate tests
/**
 * Test sin_gen_populate for invalid inputs
 */
void test_populate_invalid(void) {
  TEST_ASSERT_EQUAL(SIN_GEN_INVALID, sin_gen_populate(NULL, dummy_buf, dummy_buflen, 1000, 100000));
  // No request test is possible here

  TEST_ASSERT_EQUAL(SIN_GEN_INVALID, sin_gen_populate(test_req, NULL, dummy_buflen, 1000, 100000));
  TEST_ASSERT_SG_REQ_EQ(NULL, dummy_buflen, 0, 1, 1000, 100000);

  TEST_ASSERT_EQUAL(SIN_GEN_INVALID, sin_gen_populate(test_req, dummy_buf, 0, 1000, 100000));
  TEST_ASSERT_SG_REQ_EQ(dummy_buf, 0, 0, 1, 1000, 100000);

  TEST_ASSERT_EQUAL(SIN_GEN_INVALID, sin_gen_populate(test_req, dummy_buf, dummy_buflen, 0, 100000));
  TEST_ASSERT_SG_REQ_EQ(dummy_buf, dummy_buflen, 0, 1, 0, 100000);

  TEST_ASSERT_EQUAL(SIN_GEN_INVALID, sin_gen_populate(test_req, dummy_buf, dummy_buflen, 1000, 0));
  TEST_ASSERT_SG_REQ_EQ(dummy_buf, dummy_buflen, 0, 1, 1000, 0);
}

/**
 * Test sin_gen_populate for happy path
 */
void test_populate_happy_path(void) {
  TEST_ASSERT_EQUAL(SIN_GEN_OKAY, sin_gen_populate(test_req, dummy_buf, dummy_buflen, 1000, 100000));
  TEST_ASSERT_SG_REQ_EQ(dummy_buf, dummy_buflen, 0, 1, 1000, 100000);
}

//////////////////////////////
// sin_gen_sin tests

void test_sin(void) {

  // Smoke test of "zeros" (we have a 127 unit DC offset)
  TEST_ASSERT_EQUAL(127, sin_gen_sin(0.0, 1));
  TEST_ASSERT_EQUAL(127, sin_gen_sin(3.14, 1));
  TEST_ASSERT_EQUAL(127, sin_gen_sin(6.28, 1));
  TEST_ASSERT_EQUAL(127, sin_gen_sin(-6.28, 1));
  TEST_ASSERT_EQUAL(127, sin_gen_sin(-628.32, 1));


  char errmsg[256];
  for (int i = 0; i < 1024; i++) {
    float theta = COS_THETA0 * i / 256;

    int quadrant = i / 256;
    int offset = i % 256;

    if (quadrant % 2) offset = 255 - offset;

    uint8_t expected = expected_sin_table[offset];

    expected = 127 + (quadrant > 1 ? -expected : expected); // yuck yet yum.

    uint8_t got = sin_gen_sin(theta, 1);

    snprintf(errmsg, 255, "Case %d/%0.3f (%d)", i, theta, offset);

    TEST_ASSERT_EQUAL_MESSAGE(expected, got, errmsg);
  }
}


void test_sin_scaled(void) {

  // Smoke test of "zeros" (we have a 127 unit DC offset)
  TEST_ASSERT_EQUAL(127, sin_gen_sin(0.0, 2));
  TEST_ASSERT_EQUAL(127, sin_gen_sin(3.14, 4));
  TEST_ASSERT_EQUAL(127, sin_gen_sin(6.28, 8));
  TEST_ASSERT_EQUAL(127, sin_gen_sin(-6.28, 127));
  TEST_ASSERT_EQUAL(127, sin_gen_sin(-628.32, 255));


  char errmsg[256];

  for (uint8_t scale = 2; scale < 200; scale++) {
    for (int i = 0; i < 1024; i++) {
      float theta = COS_THETA0 * i / 256;

      int quadrant = i / 256;
      int offset = i % 256;

      if (quadrant % 2) offset = 255 - offset;

      uint8_t expected = expected_sin_table[offset] / scale;

      expected = 127 + (quadrant > 1 ? -expected : expected); // yuck yet yum.

      uint8_t got = sin_gen_sin(theta, scale);

      snprintf(errmsg, 255, "Scale=%d case %d/%0.3f (%d)", scale, i, theta, offset);

      TEST_ASSERT_EQUAL_MESSAGE(expected, got, errmsg);
    }
  }
}

//////////////////////////////
// sin_gen_generate tests
/**
 * Check that invalid calls explode appropriately
 */
void test_generate_invalid_requests(void) {
  sin_gen_result_t res;

  // NULL request case
  res = sin_gen_generate(NULL);
  TEST_ASSERT_EQUAL(SIN_GEN_INVALID, res);

  // NULL buffer case


  // Zero buflen case
}


/**
 * Check that undersampled requests are handled
 */
void test_generate_undersampling(void) {
  // Check right at the nyquist frequency
  TEST_ASSERT_EQUAL(SIN_GEN_OKAY, sin_gen_populate(test_req, dummy_buf, dummy_buflen, 1000, 2000));
  TEST_ASSERT_EQUAL(SIN_GEN_UNDERSAMPLED, sin_gen_generate(test_req));

  // Check right at the nyquist frequency, but with rounding this time
  TEST_ASSERT_EQUAL(SIN_GEN_OKAY, sin_gen_populate(test_req, dummy_buf, dummy_buflen, 1000, 2001));
  TEST_ASSERT_EQUAL(SIN_GEN_UNDERSAMPLED, sin_gen_generate(test_req));

  // Check right above the nyquist frequency (not a good idea, but it should work)
  TEST_ASSERT_EQUAL(SIN_GEN_OKAY, sin_gen_populate(test_req, dummy_buf, dummy_buflen, 1000, 2002));
  TEST_ASSERT_EQUAL(SIN_GEN_OKAY, sin_gen_generate(test_req));
}


/**
 * Check that too short of buffer is caught.
 *
 * Note the literal edge cases.
 */
void test_generate_too_short(void) {
  uint16_t buflen = 4; // Make our math easier for this case

  // Test an obviously bad case
  TEST_ASSERT_EQUAL(SIN_GEN_OKAY, sin_gen_populate(test_req, dummy_buf, buflen, 1, 2000));
  TEST_ASSERT_EQUAL(SIN_GEN_TOO_SHORT, sin_gen_generate(test_req));


  // This should succeed perfectly
  TEST_ASSERT_EQUAL(SIN_GEN_OKAY, sin_gen_populate(test_req, dummy_buf, buflen, 500, 2000));
  TEST_ASSERT_EQUAL(SIN_GEN_OKAY, sin_gen_generate(test_req));
  TEST_ASSERT_SG_RES_EQ(4, 0);
  uint8_t expected_wav[4] = {127, 254, 127, 0};
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected_wav, test_req->buf, 4);

  // Take away one sample: should be too short by a quarter wave
  TEST_ASSERT_EQUAL(SIN_GEN_OKAY, sin_gen_populate(test_req, dummy_buf, 3, 500, 2000));
  TEST_ASSERT_EQUAL(SIN_GEN_TOO_SHORT, sin_gen_generate(test_req));
  TEST_ASSERT_SG_RES_EQ(3, COS_THETA0);

  // And let's go for math that's a bit gnarlier: good case
  TEST_ASSERT_EQUAL(SIN_GEN_OKAY, sin_gen_populate(test_req, dummy_buf, 1000, 100, 100000));
  TEST_ASSERT_EQUAL(SIN_GEN_OKAY, sin_gen_generate(test_req));
  TEST_ASSERT_SG_RES_EQ(1000, 0);

  // And let's go for math that's a bit gnarlier: one sample short
  TEST_ASSERT_EQUAL(SIN_GEN_OKAY, sin_gen_populate(test_req, dummy_buf, 999, 100, 100000));
  TEST_ASSERT_EQUAL(SIN_GEN_TOO_SHORT, sin_gen_generate(test_req));
  TEST_ASSERT_SG_RES_EQ(999, COS_THETA0/250);
}

/**
 * Run a happy-path request, mostly as a smoke test
 */
void test_generate_happy_path(void) {

  // Generate a 1:1 copy of our sine wave table
  uint8_t expected_wav[1024];

  for (int i = 0; i < 1024; i++) {
    int quadrant = i / 256;
    int offset = i % 256;

    if (quadrant % 2) offset = 255 - offset;

    int8_t j = expected_sin_table[offset];
    expected_wav[i] = 127 + (quadrant > 1 ? -j : j);
  }

  TEST_ASSERT_EQUAL(SIN_GEN_OKAY, sin_gen_populate(test_req, dummy_buf, 1024, 1, 1024));
  TEST_ASSERT_SG_REQ_EQ(dummy_buf, dummy_buflen, 0, 1, 1, 1024);

  TEST_ASSERT_EQUAL(SIN_GEN_OKAY, sin_gen_generate(test_req));
  TEST_ASSERT_SG_RES_EQ(1024, 0);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected_wav, test_req->buf, 1024);
}


/**
 * Run a happy-path request, mostly as a smoke test
 */
void test_generate_fill__happy_path(void) {

  // Generate a 1:1 copy of our sine wave table
  uint8_t expected_wav[1024];

  for (int i = 0; i < 1024; i++) {
    int k = (4*i) % 1024;
    int quadrant = k / 256;
    int offset = k % 256;

    if (quadrant % 2) offset = 255 - offset;

    int8_t j = expected_sin_table[offset];
    expected_wav[i] = 127 + (quadrant > 1 ? -j : j);
  }

  TEST_ASSERT_EQUAL(SIN_GEN_OKAY, sin_gen_populate(test_req, dummy_buf, 1024, 4, 1024));
  TEST_ASSERT_SG_REQ_EQ(dummy_buf, dummy_buflen, 0, 1, 4, 1024);

  TEST_ASSERT_EQUAL(SIN_GEN_OKAY, sin_gen_generate_fill(test_req));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected_wav, test_req->buf, 1024);
}

/**
 * Test clean integer downsampling of our table
 */
void test_generate_downsample2(void) {

  // Generate a 1:1 copy of our sine wave table
  uint8_t expected_wav[1024];

  // 0xFF is unattainable by our sine generator, so makes a good
  // sentinel for testing.
  memset(expected_wav, 0xff, 1024);
  memset(test_req->buf, 0xff, 1024);

  for (int i = 0; i < 512; i++) {
    int quadrant = i / 128;
    int offset = (2*i) % 256;

    if (quadrant % 2) offset = 255 - offset;

    int8_t j = expected_sin_table[offset];
    expected_wav[i] = 127 + (quadrant > 1 ? -j : j);
  }

  TEST_ASSERT_EQUAL(SIN_GEN_OKAY, sin_gen_populate(test_req, dummy_buf, 512, 2, 1024));
  TEST_ASSERT_SG_REQ_EQ(dummy_buf, 512, 0, 1, 2, 1024);

  TEST_ASSERT_EQUAL(SIN_GEN_OKAY, sin_gen_generate(test_req));
  TEST_ASSERT_SG_RES_EQ(512, 0);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected_wav, test_req->buf, 1024);
}


/**
 * Test messy integer downsampling of our table
 */
void test_generate_downsample3(void) {

  // Generate a 1:1 copy of our sine wave table
  uint8_t expected_wav[1024];

  memset(expected_wav, ' ', 1024);
  expected_wav[1023] = 0;

  // There's a gnarly rounding problem at the halfway point of this
  // waveform generation.  We should get offset 513.000, but we get
  // 513.5003 due to accumulated error.  This variable lets us bump
  // that error term so we get the expected results in light of the
  // constraints of this platform.
  int cumulative_error = 0;

  for (int i = 0; i < 341; i++) {
    if (i == 171) cumulative_error++;
    int quadrant = (3*i) / 256;
    int offset = (3*i+cumulative_error) % 256;

    if (quadrant % 2) offset = 255 - offset;

    int8_t j = expected_sin_table[offset];
    expected_wav[i] = 127 + (quadrant > 1 ? -j : j);
  }

  TEST_ASSERT_EQUAL(SIN_GEN_OKAY, sin_gen_populate(test_req, dummy_buf, 1024, 3, 1024));
  TEST_ASSERT_SG_REQ_EQ(dummy_buf, 1024, 0, 1, 3, 1024);

  TEST_ASSERT_EQUAL(SIN_GEN_OKAY, sin_gen_generate(test_req));
  TEST_ASSERT_SG_RES_EQ(341, 0.00607457);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected_wav, test_req->buf, 341);
}

//////////////////////////////////////////////////////////////////////
// Actual test runner

int main(int argc, char *argv[]) {
  UNITY_BEGIN();

  RUN_TEST(test_populate_invalid);
  RUN_TEST(test_populate_happy_path);

  RUN_TEST(test_sin);
  RUN_TEST(test_sin_scaled);

  RUN_TEST(test_generate_invalid_requests);
  RUN_TEST(test_generate_undersampling);
  RUN_TEST(test_generate_too_short);

  RUN_TEST(test_generate_happy_path);
  RUN_TEST(test_generate_fill__happy_path);
  RUN_TEST(test_generate_downsample2);
  RUN_TEST(test_generate_downsample3);

  return UNITY_END();
}
