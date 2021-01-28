#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
/**
 * \file tamo_state.h
 * \brief Header for tamodevboard mental states
 */

typedef enum tamo_emotion {
			   TAMO_UNKNOWN=0,  //!< A sentinel value for uninitialized values
			   TAMO_LONELY,  //!< Tamodevboard is lonely
			   TAMO_HAPPY, //!< Tamodevboard is happy to see people
			   TAMO_BORED, //!< Tamodevboard is done with people now
} tamo_emotion_t;

#define TAMO_INTERACTION_TIMEOUT 3  //! How much human interaction before tamodevboard gets bored (seconds)
#define TAMO_INTROSPECTION_TIMEOUT 5  //! How long before the tamodevboard realizes it's lonely (seconds) 

typedef struct tamo_state {
  uint32_t last_timestamp;
  tamo_emotion_t current_emotion;
} tamo_state_t;


const char *tamo_emotion_name(tamo_emotion_t);

void tamo_state_init(tamo_state_t *, uint32_t);
bool tamo_state_update(tamo_state_t *, uint32_t, bool);
tamo_emotion_t tamo_state_compute_next(tamo_emotion_t current_emotion, int32_t dt, bool user_present);
