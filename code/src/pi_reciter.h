#pragma once

/**
 * \file pi_reciter.h
 * \brief Pi reciter header
 */

#include <stdint.h>

/**
 * \addtogroup pi_reciter
 * \{
 *
 */

/**
 * Return values for pi_reciter_rx_digit()
 */
typedef enum pi_reciter_rx_state {
				  PI_RECITER_OKAY = 0,  //!< Got the right digit back
				  PI_RECITER_WRONG_DIGIT, //!< Got back an incorrect digit
				  PI_RECITER_EXHAUSTED, //!< Out of digits in our table
} pi_reciter_rx_state_t;

void pi_reciter_init(void);
uint8_t pi_reciter_next_digit(void);
pi_reciter_rx_state_t pi_reciter_rx_digit(uint8_t);
void pi_reciter_reset(void);

/** \} */ // End doxygen group
