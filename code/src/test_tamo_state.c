#include <stdint.h>

#include "unity.h"

#include "tamo_state.h"

//! Our test dummy
static tamo_state_t test_subject;

//! An initial timestamp for our devboard
static const uint32_t t0 = 554398500;


//////////////////////////////////////////////////////////////////////
// Unity requires a setUp and tearDown function.

/**
 * Reset the state of our tamodevboard as if it booted up at t0
 */
void setUp(void) { tamo_state_init(&test_subject, t0); }

/**
 * There is nothing to tear down in this set of tests.
 */
void tearDown(void) {}

//////////////////////////////////////////////////////////////////////
// Some helper macros to make state transitions easier to check on

/**
 * \brief Test that the test_subject's state matches expectations
 */
#define TEST_ASSERT_TAMO_STATE(expected_timestamp, expected_emotion)           \
  {                                                                            \
    TEST_ASSERT_EQUAL(expected_timestamp, test_subject.last_timestamp);        \
    TEST_ASSERT_EQUAL(expected_emotion, test_subject.current_emotion);         \
  }

/**
 * \brief Run an input through the state machine and check results
 *
 * Given the new timestamp and user_present value, check for an
 * expected change in state, and then ensure that the timestamp and
 * resulting tamodevboard emotional state matches.
 *
 * This is a macro to ensure that __LINE__ works as expected (doing
 * this as a function messes with the line number output, which messes
 * with the ability of tools to help you zero in on errors).
 */
#define TEST_ASSERT_TAMO_EDGE(timestamp, user_present, expected_change,        \
                              expected_timestamp, expected_emotion)            \
  {                                                                            \
    TEST_ASSERT_EQUAL(                                                         \
        expected_change,                                                       \
        tamo_state_update(&test_subject, timestamp, user_present));            \
    TEST_ASSERT_TAMO_STATE(expected_timestamp, expected_emotion);              \
  }

#define CHANGE_YES true
#define CHANGE_NO false

#define USER_IS_PRESENT true
#define USER_NOT_PRESENT false

//////////////////////////////////////////////////////////////////////
// The test case implementations

/**
 * Test that our initial state matches expectations.  Most of the test
 * cases call this to ensure that they are starting in the correct
 * state.
 */
void test_initial_state(void) { TEST_ASSERT_TAMO_STATE(t0, TAMO_LONELY); }


/**
 * Ensure that tamodevboard stays in LONELY when it doesn't see a
 * person.
 */
void test_lonely_to_lonely(void) {
  test_initial_state();

  // Nothing changes, so still lonely
  TEST_ASSERT_TAMO_EDGE(t0 + 10, USER_NOT_PRESENT, CHANGE_NO, t0, TAMO_LONELY);
}


/**
 * When a person is seen in LONELY, should go to HAPPY immediately
 */
void test_lonely_to_happy(void) {
  const uint32_t t_now = t0 + 10; // Arbitrary time

  test_initial_state();

  // Got a user visit at t_now
  TEST_ASSERT_TAMO_EDGE(t_now, USER_IS_PRESENT, CHANGE_YES, t_now, TAMO_HAPPY);
}


/**
 * If the person leaves before we get BORED, go to LONELY
 */
void test_happy_to_lonely(void) {
  const uint32_t t_started = t0 + 10;
  const uint32_t t_now = t_started + 2;
  // XXX This should be connected to the specification somehow

  test_initial_state();

  // Got a user visit at t_started
  TEST_ASSERT_TAMO_EDGE(t_started, USER_IS_PRESENT, CHANGE_YES, t_started,
                        TAMO_HAPPY);

  // And then they leave at t_now
  TEST_ASSERT_TAMO_EDGE(t_now, USER_NOT_PRESENT, CHANGE_YES, t_now,
                        TAMO_LONELY);
}

/**
 * If a spurious "user present" signal is received before the BORED
 * timeout, should remain in HAPPY with no change.
 */
void test_happy_to_happy(void) {
  const uint32_t t_started = t0 + 10;
  const uint32_t t_now = t_started + 2;
  // XXX This should be connected to the specification somehow

  test_initial_state();
  // Got a user visit at t_started
  TEST_ASSERT_TAMO_EDGE(t_started, USER_IS_PRESENT, CHANGE_YES, t_started,
                        TAMO_HAPPY);

  // And then they retrigger presence detect at t_now
  TEST_ASSERT_TAMO_EDGE(t_now, USER_IS_PRESENT, CHANGE_NO, t_started,
                        TAMO_HAPPY);
}


/**
 * Once the person has been around long enough, we should go from
 * HAPPY to BORED.
 */
void test_happy_to_bored(void) {
  const uint32_t t_started = t0 + 10;
  const uint32_t t_now = t_started + 10;
  // XXX This should be connected to the specification somehow

  test_initial_state();
  // Got a user visit at t_started
  TEST_ASSERT_TAMO_EDGE(t_started, USER_IS_PRESENT, CHANGE_YES, t_started,
                        TAMO_HAPPY);

  // And then they retrigger presence detect at t_now
  TEST_ASSERT_TAMO_EDGE(t_now, USER_IS_PRESENT, CHANGE_YES, t_now, TAMO_BORED);
}


/**
 * If the user leaves while we're BORED, but comes back before
 * introspection kicks in and we become LONELY, we should reset the
 * BORED timer (introspection was ruined).
 */
void test_happy_to_bored_revisit(void) {
  const uint32_t t_started = t0 + 10;
  const uint32_t t_now = t_started + 5;
  // XXX This should be connected to the specification somehow
  const uint32_t t_revisit = t_now + 1; // XXX Another thing to tie into specs

  test_initial_state();

  // Got a user visit at t_started
  TEST_ASSERT_TAMO_EDGE(t_started, USER_IS_PRESENT, CHANGE_YES, t_started,
                        TAMO_HAPPY);

  // Then they leave at t_now, *without an intervening trigger*
  TEST_ASSERT_TAMO_EDGE(t_now, USER_NOT_PRESENT, CHANGE_YES, t_now, TAMO_BORED);

  // And come back at t_revisit
  TEST_ASSERT_TAMO_EDGE(t_revisit, USER_IS_PRESENT, CHANGE_NO, t_revisit,
                        TAMO_BORED);
}


/**
 * If we're BORED and don't get any interruptions, should go to LONELY
 * after an introspection timeout.
 */
void test_bored_to_lonely_direct(void) {
  // Test a direct transition to lonely out of bored.
  const uint32_t t_started = t0 + 10;
  const uint32_t t_now =
      t_started +
      5; // XXX This should be connected to the specification somehow
  const uint32_t t_revisit = t_now + 10; // XXX Another thing to tie into specs

  test_initial_state();

  // Got a user visit at t_started
  TEST_ASSERT_TAMO_EDGE(t_started, USER_IS_PRESENT, CHANGE_YES, t_started,
                        TAMO_HAPPY);

  // Then they leave at t_now, *without an intervening trigger*
  TEST_ASSERT_TAMO_EDGE(t_now, USER_NOT_PRESENT, CHANGE_YES, t_now, TAMO_BORED);

  // And then we check in at t_revisit, after introspection has kicked in
  TEST_ASSERT_TAMO_EDGE(t_revisit, USER_NOT_PRESENT, CHANGE_YES, t_revisit,
                        TAMO_LONELY);
}


/**
 * Ensure that our UNKNOWN to LONELY/HAPPY transition works for the LONELY case
 */
void test_unknown_to_lonely(void) {
  // Ensure that our safety valve works

  uint32_t t_started = t0 + 1;

  test_initial_state();
  test_subject.current_emotion = TAMO_UNKNOWN;

  TEST_ASSERT_TAMO_EDGE(t_started, USER_NOT_PRESENT, CHANGE_YES, t_started,
                        TAMO_LONELY);
}


/**
 * Ensure that our UNKNOWN to LONELY/HAPPY transition works for the HAPPY case
 */
void test_unknown_to_happy(void) {
  // Ensure that our safety valve works

  uint32_t t_started = t0 + 1;

  test_initial_state();
  test_subject.current_emotion = TAMO_UNKNOWN;
  TEST_ASSERT_TAMO_EDGE(t_started, USER_IS_PRESENT, CHANGE_YES, t_started,
                        TAMO_HAPPY);
}


/**
 * Ensure that our invalid state to LONELY/HAPPY transition works for
 * the LONELY case
 */

void test_invalid_to_lonely(void) {
  // Ensure that our safety valve works

  uint32_t t_started = t0 + 1;

  test_initial_state();
  test_subject.current_emotion = 42;
  TEST_ASSERT_TAMO_EDGE(t_started, USER_NOT_PRESENT, CHANGE_YES, t_started,
                        TAMO_LONELY);
}


/**
 * Ensure that our time travel handler pulls into the LONELY case when
 * appropriate.
 */
void test_time_travel_to_lonely(void) {
  uint32_t t_started = t0 + 10;
  uint32_t t_warp = t0 - 86400;

  test_initial_state();

  TEST_ASSERT_TAMO_EDGE(t_started, USER_IS_PRESENT, CHANGE_YES, t_started,
                        TAMO_HAPPY);
  TEST_ASSERT_TAMO_EDGE(t_warp, USER_NOT_PRESENT, CHANGE_YES, t_warp,
                        TAMO_LONELY);
}


/**
 * Ensure that our time travel handler pulls into the HAPPY case when
 * appropriate.
 */
void test_time_travel_to_happy(void) {
  uint32_t t_started = t0 + 10;
  uint32_t t_warp = t0 - 86400;

  test_initial_state();

  TEST_ASSERT_TAMO_EDGE(t_started, USER_IS_PRESENT, CHANGE_YES, t_started,
                        TAMO_HAPPY);
  TEST_ASSERT_TAMO_EDGE(t_warp, USER_IS_PRESENT, CHANGE_YES, t_warp,
                        TAMO_HAPPY);
}

//////////////////////////////////////////////////////////////////////
// Actual test runner

int main(int argc, char *argv[]) {
  UNITY_BEGIN();
  RUN_TEST(test_initial_state);
  RUN_TEST(test_lonely_to_lonely);
  RUN_TEST(test_lonely_to_happy);
  RUN_TEST(test_happy_to_lonely);
  RUN_TEST(test_happy_to_happy);
  RUN_TEST(test_happy_to_bored);
  RUN_TEST(test_happy_to_bored_revisit);
  RUN_TEST(test_bored_to_lonely_direct);

  // Now check our "invalid state" recovery mechanisms
  RUN_TEST(test_unknown_to_lonely);
  RUN_TEST(test_unknown_to_happy);
  RUN_TEST(test_invalid_to_lonely);
  RUN_TEST(test_time_travel_to_lonely);
  RUN_TEST(test_time_travel_to_happy);

  return UNITY_END();
}
