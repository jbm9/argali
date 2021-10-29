#include <stdint.h>
#include <string.h>

#include "unity.h"

#include "pi_reciter.h"

//! Our test dummy


// Fixture for our table

#define N_PI_DIGITS 128 //!< Number of digits of pi TamoDevBoard has memorized

/** The actual digits of pi, recited in DTMF when it's bored. */
static uint8_t PI_DIGITS[N_PI_DIGITS] =		\
  "31415926535897932384626433832795"		\
  "02884197169399375105820974944592"		\
  "30781640628620899862803482534211"		\
  "70679821480865132823066470938446";



//////////////////////////////////////////////////////////////////////
// Unity requires a setUp and tearDown function.

/**
 * Blank out our request
 */
void setUp(void) {

}

/**
 * There is nothing to tear down in this set of tests.
 */
void tearDown(void) {}

//////////////////////////////////////////////////////////////////////
// Helper macros


/**
 * Run a single transaction, with the expected symbol next, and
 * expected state next
 */
void run_next_transaction(const char* funcname,
			  char *err_header,
			  uint8_t expected_sym,
			  pi_reciter_rx_state_t expected_state) {

  char errmsg[256];

  snprintf(errmsg, 255, "%s %s: next symbol", funcname, err_header);
  uint8_t got = pi_reciter_next_digit();
  TEST_ASSERT_EQUAL_MESSAGE(expected_sym, got, errmsg);

  snprintf(errmsg, 255, "%s %s: next state", funcname, err_header);
  pi_reciter_rx_state_t s = pi_reciter_rx_digit(got);
  TEST_ASSERT_EQUAL_MESSAGE(expected_state, s, errmsg);
}



/**
 * Encapsulates the logic of running n successful cases
 *
 * This fetchs n symbols from the pi_reciter, and sends them all back
 * as clean copy.  After calling this, you should be clear to get the
 * n+1'th symbol back and go from there.
 *
 * This replaces a giant whap of copypasta that was getting extremely
 * ugly to look at over and over and over and over again.
 *
 * This does not call pi_reciter_init() or pi_reciter_reset().
 *
 * This will only run a million transactions for you, beyond that,
 * you're on your own.  Sorry.
 *
 */
static void run_n_successes(const char *funcname, const char *err_header, int n) {
  // This encapsulates the logic for a complete flow of n fetches and returns.
  char errmsg[256];

  for (uint8_t my_cursor = 0; my_cursor < N_PI_DIGITS; my_cursor++) {
    // If we've done n cases, bail out
    if (my_cursor == n) {
      return;
    }

    snprintf(errmsg, 255, "%s: digit %d", err_header, my_cursor);
    pi_reciter_rx_state_t next_state;

    if (my_cursor != N_PI_DIGITS-1) {
      next_state = PI_RECITER_OKAY;
    } else {
      next_state = PI_RECITER_EXHAUSTED;
    }

    run_next_transaction(funcname, errmsg, PI_DIGITS[my_cursor], next_state);
  }

  // Get a lot of exhaustion cases, to make sure we aren't
  // accidentally wrapping around the array
  for (int i = N_PI_DIGITS; i < 1000000; i++) {
    if (i == n) {
      return;
    }

    snprintf(errmsg, 255, "%s: exhaustion loop %d", err_header, i);
    run_next_transaction(funcname, errmsg, '#', PI_RECITER_EXHAUSTED);
  }
}


//////////////////////////////////////////////////////////////////////
// The test case implementations


/**
 * Tests that the run_n_successes() helper is working as expected
 */
void test_run_n_successes(void) {
  pi_reciter_init();

  run_n_successes(__func__,  " initial runup", N_PI_DIGITS-1); // Leave one more transaction

  run_next_transaction(__func__,  " First exhaustion", PI_DIGITS[N_PI_DIGITS-1], PI_RECITER_EXHAUSTED);

  // Now test a giant run, and make sure we're in EXHAUSTED
  pi_reciter_reset();
  run_n_successes(__func__,  " runup after reset", 1000);

  run_next_transaction(__func__,  " check after reset", '#', PI_RECITER_EXHAUSTED);
}
void test_exhaustion(void) {
  char errmsg[256];
  uint8_t got;
  pi_reciter_rx_state_t s;

  pi_reciter_init();

  ////////////////////
  // This is literally the same as test_run_n_successes()
  run_n_successes(__func__,  " initial runup", N_PI_DIGITS-1); // Leave one more transaction
  got = pi_reciter_next_digit();
  TEST_ASSERT_EQUAL(PI_DIGITS[N_PI_DIGITS-1], got);

  s = pi_reciter_rx_digit(got);
  TEST_ASSERT_EQUAL(PI_RECITER_EXHAUSTED, s);

  // Now test a giant run, and make sure we're in EXHAUSTED
  pi_reciter_reset();
  run_n_successes(__func__,  " run to exhaustion", 1000);
  got = pi_reciter_next_digit();
  TEST_ASSERT_EQUAL('#', got);

  s = pi_reciter_rx_digit(got);
  TEST_ASSERT_EQUAL(PI_RECITER_EXHAUSTED, s);

  ////////////////////
  // Reset state
  pi_reciter_reset();

  // And do it all over again
  run_n_successes(__func__,  " second runup", N_PI_DIGITS-1); // Leave one more transaction


  // Now test a giant run, and make sure we're in EXHAUSTED
  pi_reciter_reset();

  run_n_successes(__func__,  " second run to exhaustion", 1000);

  run_next_transaction(__func__, " second run end", '#', PI_RECITER_EXHAUSTED);
}

void test_wrong_digit__at_end(void) {
  char errmsg[256];
  pi_reciter_init();

  // We test what happens if we get an incorrect digit in the last
  // place, as that's a weird corner case.  It has a clear logical
  // definition, but it's easy to mess up the order of operations and
  // accidentally go to EXHAUSTED instead of WRONG_DIGIT.

  uint8_t final_digit = PI_DIGITS[N_PI_DIGITS-1];

  run_n_successes(__func__, "runup", N_PI_DIGITS-1);

  // Make sure we're at the end
  uint8_t got = pi_reciter_next_digit();
  TEST_ASSERT_EQUAL(final_digit, got); // Make sure we're at the end

  // And now kick in a wrong digit
  pi_reciter_rx_state_t s = pi_reciter_rx_digit('B');
  TEST_ASSERT_EQUAL(PI_RECITER_WRONG_DIGIT, s);

  // Get a lot of wrong digit cases, to make sure we aren't
  // accidentally wrapping around the array
  for (int i = 0; i < 10000; i++) {
    snprintf(errmsg, 255, "wrong digit loop %d", i);
    run_next_transaction(__func__, errmsg, 'A', PI_RECITER_WRONG_DIGIT);
  }

  // And make sure that reset recovers and cleanly transitions to exhausted
  pi_reciter_reset();

  run_n_successes(__func__, "post reset", 1000);

}

void test_next_digit_repeated(void) {
  // Make sure that next digit doesn't silently auto-advance
  pi_reciter_init();

  for (int i = 0; i < 1000; i++) {
    uint8_t got = pi_reciter_next_digit();

    // Forgive me for hard-coding the first digit of pi
    TEST_ASSERT_EQUAL('3', got);
  }

  TEST_ASSERT_EQUAL(PI_RECITER_OKAY, pi_reciter_rx_digit('3'));

  for (int i = 0; i < 1000; i++) {
    uint8_t got = pi_reciter_next_digit();

    // ... and the second one
    TEST_ASSERT_EQUAL('1', got);
  }
}

//////////////////////////////////////////////////////////////////////
// Actual test runner

int main(int argc, char *argv[]) {
  UNITY_BEGIN();

  // Who tests our test helper?  We do.
  RUN_TEST(test_run_n_successes);

  RUN_TEST(test_exhaustion);
  RUN_TEST(test_wrong_digit__at_end);
  RUN_TEST(test_next_digit_repeated);

  return UNITY_END();
}
