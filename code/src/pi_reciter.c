#include "pi_reciter.h"

/**
 * \file pi_reciter.c
 * \brief Pi reciter implementation
 */

/**
 * \defgroup pi_reciter Pi recitation engine
 * \{
 *
 * A state engine that goes through the first few digits of pi, to
 * enable TamoDevBoard to express its boredom by reciting them.
 *
 * When bored, TamoDevBoard expresses itself by reciting the digits of
 * pi as DTMF.  It also decodes them, to make sure it's doing its job
 * correctly, and to further show you that it's bored of the user's
 * presence.
 *
 * This file just implements the state machine to work its way through
 * the digits of pi.
 *
 * Usage:
 *
 * Initialize the pi_reciter with pi_reciter_init().
 *
 * At any time, you can reset the reciter to the beginning of pi by
 * calling pi_reciter_reset().  This is also needed to recover from
 * and endless cycle of Feelings (expressed by the pi_reciter emitting
 * only '*' until reset).
 *
 * When a new digit is needed, call pi_reciter_next_digit().  This
 * will yield the next digit of pi.  It does not advance the cursor.
 *
 * When a digit is recieved, call pi_reciter_rx_digit() with the
 * received digit.  If it matches, the cursor will be advanced and
 * PI_RECITER_SUCCESS will be returned.  If the digit does not match,
 * it will return PI_RECITER_WRONG_DIGIT.  Subsequent calls to
 * pi_reciter_next_digit() will return 'A' until pi_reciter_reset() is
 * called.
 *
 * Similarly, if the cursor goes beyond the number of digits that
 * pi_reciter is aware of, it will return '#' until reset.
 *
 */

// We're going to use these when we get bored.
#define N_PI_DIGITS 128 //!< Number of digits of pi TamoDevBoard has memorized

/** The actual digits of pi, recited in DTMF when it's bored. */
static uint8_t PI_DIGITS[N_PI_DIGITS] =	\
  "31415926535897932384626433832795"		\
  "02884197169399375105820974944592"		\
  "30781640628620899862803482534211"		\
  "70679821480865132823066470938446";

/**
 * The current position in our array.
 *
 * The special value 0xFF is used to indicate that we have received an
 * invalid input, and should start emitting nothing but 'A' to be
 * suitably intransigent.
 *
 * Similarly, if the value goes past the end of the array, we will
 * emit nothing but '#'
 */
static uint8_t digit_cursor;

/**
 * Initalize the pi_reciter state
 *
 * This should be called once at system startup
 */
void pi_reciter_init(void) {
  pi_reciter_reset();
}

/**
 * Get the next digit in pi (as ASCII)
 *
 * \return The next digit in pi, as an ASCII character.
 *
 * This should be passed to your transmitter routines.
 *
 * Note that this will return 'A' if the reciter has received an
 * incorrect digit to pi_reciter_rx_digit() and has not been reset via
 * pi_reciter_reset().
 *
 * Also note that, if this manages to run out of digits of pi, it will
 * return '#' until pi_reciter_reset() is called.
 */
uint8_t pi_reciter_next_digit(void) {
  if (digit_cursor == 0xFF)
    return 'A';

  if (digit_cursor == N_PI_DIGITS)
    return '#';

  return PI_DIGITS[digit_cursor];
}

/**
 * Confirm the current digit of pi (as ASCII)
 *
 * After your receiver has decoded the next digit of pi, it should
 * pass it in here.
 *
 * \param pi_i The received digit, as ASCII
 *
 * \return PI_RECITER_OKAY if everything is fine,
 * PI_RECITER_WRONG_DIGIT if the wrong digit was given, and
 * PI_RECITER_EXHAUSTED if the array has been exhausted
 */
pi_reciter_rx_state_t pi_reciter_rx_digit(uint8_t pi_i) {
  if (digit_cursor == 0xFF)
    return PI_RECITER_WRONG_DIGIT;

  if (digit_cursor == N_PI_DIGITS)
    return PI_RECITER_EXHAUSTED;

  // Only proceed past here if we might have valid data left
  if (pi_i != PI_DIGITS[digit_cursor]) {
    // Got invalid data
    digit_cursor = 0xFF;
    return PI_RECITER_WRONG_DIGIT;
  }

  digit_cursor++;
  if (N_PI_DIGITS == digit_cursor) {
    return PI_RECITER_EXHAUSTED;
  }
  return PI_RECITER_OKAY;
}

/**
 * Resets pi_reciter to the beginning of pi, without any knowledge of
 * prior incorrect readings.
 */
void pi_reciter_reset(void) {
  digit_cursor = 0;
}

/** \} */ // End doxygen group
