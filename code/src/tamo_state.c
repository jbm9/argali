#include "tamo_state.h"

/**
 * \file tamo_state.c
 * \brief Implements the tamodevboard happiness states
 *
 * \defgroup tamo_state Tamodevboard's mental state
 * \{
 */

/**
 * \subsection tamo_state_theory Theory of Operation
 *
 * Your devboard has a somewhat rich inner life.  It is lonely when no
 * one is around, but when it detects that a person is nearby, it
 * becomes happy for a while.  But if that person is around too long,
 * it gets bored.  While a person is around, it remains bored.  After
 * the person has left, however, it will eventually become lonely
 * again.
 *
 * 
 */

/**
 * \brief Translate an emotion into a human name
 *
 * \param emotion The emotion state to get the name of
 *
 * \return NULL if the state is unknown, otherwise a pointer to the appropriate string.
 *
 * It may be beneficial to not return NULL here, and instead just
 * return "!INVALID!" or some such.  This makes debugging printf()s a
 * lot easier to put together.
 */
const char *tamo_emotion_name(tamo_emotion_t emotion)
{
  switch(emotion) {
  case TAMO_UNKNOWN: return "UNKNOWN";
  case TAMO_LONELY: return "Lonely.";
  case TAMO_HAPPY: return "Happy!";
  case TAMO_BORED: return "Bored?";
  default: return NULL;
  }
}

/**
 * \brief Initialize the tamadeboard mental state
 *
 * \param tst A pointer to the tamo_state_t variable to re-initialize
 * \param timestamp The timestamp of initialization
 */
void tamo_state_init(tamo_state_t *tst, uint32_t timestamp)
{
  if (NULL == tst) return;  // Short-circuit this
  tst->last_timestamp = timestamp;
  tst->current_emotion = TAMO_LONELY;
}


/**
 * \brief (Internal) Compute the next emotional state given the relevant data
 *
 * \param current_emotion The current emotion our tamodevboard is in
 * \param dt Time since the tamodevboard entered that state
 * \param user_present Whether or not a user is present
 *
 * \return The subsequent emotional state for tamodevboard
 */
tamo_emotion_t tamo_state_compute_next(tamo_emotion_t current_emotion, int32_t dt, bool user_present)
{
  // Our tamodevboard is distressed by undergoing time travel
  if (dt < 0) return TAMO_UNKNOWN;
  
  switch(current_emotion) {
  case TAMO_LONELY:
    return (user_present ? TAMO_HAPPY : TAMO_LONELY);
    break;

  case TAMO_HAPPY:
    // The person left while we were still happy.  Go straight to lonely.
    if (!user_present) return TAMO_LONELY;

    // If they've been around too long, we'll get bored
    if (dt > TAMO_INTERACTION_TIMEOUT) return TAMO_BORED;
    
    // Otherwise, we stay happy
    return TAMO_HAPPY;
    break;
    
  case TAMO_BORED:
    // No person around: has the tamodevboard started thinking about itself yet?    
    if (!user_present && (dt > TAMO_INTROSPECTION_TIMEOUT)) return TAMO_LONELY;
    // Otherwise, the tamodevboard remains bored.
    return TAMO_BORED;
    break;

  case TAMO_UNKNOWN: // fallthrough
  default:
    // If we're in an Unknown or invalid state, let's be honest with our caller
    return TAMO_UNKNOWN;
  }

}

/**
 * \brief Update the internal state of our tamodevboard
 *
 * \param tst A pointer to the tamo_state_t variable to update in-place
 * \param timestamp The timestamp of this latest interaction event
 * \param user_present Whether or not a person has been detected
 *
 * \return Whether or not the state was changed
 */
bool tamo_state_update(tamo_state_t *tst, uint32_t timestamp, bool user_present) {
  int32_t dt = timestamp - tst->last_timestamp;  //! \todo Check for overflow for exceptionally lonely devboards

  tamo_emotion_t next_emotion;

  next_emotion = tamo_state_compute_next(tst->current_emotion, dt, user_present);

  /* Handle the UNKNOWN state differently than others */
  if (TAMO_UNKNOWN == next_emotion) {
    next_emotion = TAMO_LONELY; // Just go with LONELY if we don't know.
    tst->last_timestamp = timestamp;
    return true;
  }
  
  if (next_emotion != tst->current_emotion) {
    tst->current_emotion = next_emotion;
    /**
     * \todo Should we set the "last timestamp" on the emotional state
     * to when tamodevboard *would* have transitioned, if we'd seen
     * it?  Right now, we log when it was observed, so there may be
     * some sloppiness in cases where we don't check in on the
     * tamodevboard's emotional state often enough.
     */
    tst->last_timestamp = timestamp;
    
    return true;
  }
  
  return false;
}

/** \} */ // End doxygen group
