#include "logging.h"
#include <stdio.h>

/**
 * \file logging.c
 * \brief Logging subystem implementation
 *
 * \defgroup logging Logging
 * \addtogroup logging
 * \{
 *
 * A bare-bones logging implementation.  It currently logs to
 * printf(), which is connected to the serial console.
 */


/**
 * \brief Get a human-readable version of a loglevel (used in log line header)
 *
 * \param loglevel The log level to decode
 *
 * \return A string pointer for the name, or "UNK" if it's not found in the existing table
 *
 */
const char* log_level_to_str(log_level_t loglevel) {
  switch(loglevel) {
  case LEVEL_FORCED: return "FORCED";
  case LEVEL_FATAL: return "FATAL";
  case LEVEL_ERROR: return "ERROR";
  case LEVEL_WARN: return "WARN";
  case LEVEL_INFO: return "INFO";
  case LEVEL_DEBUG: return "DEBUG";
  default: return "UNK";
  }
}


/**
 * \brief Log a line of text
 *
 * \param loglevel The level to assign to this message
 *
 * \param fmt A string to use in sprintf() for this messages
 *
 */
void logline(log_level_t loglevel, const char *fmt, ...) {
  char buf[1024];
  va_list argp;
  va_start(argp, fmt);
  vsprintf(buf, fmt, argp);
  va_end(argp);
  printf("[%s] %s\n", log_level_to_str(loglevel), buf);
}

void log_forced(const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  logline(LEVEL_FORCED, fmt, argp);
  va_end(argp);

}


/** \} */ // End doxygen group
