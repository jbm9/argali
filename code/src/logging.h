#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <stdarg.h>
/**
 * \file logging.h
 * \brief Header for logging
 *
 * \addtogroup logging
 * \{
 */

/**
 * \brief The log levels
 */
typedef enum log_level {
			LEVEL_FORCED = 0, //!< An absolutely required message to print
			LEVEL_FATAL = 1,  //!< A fatal error, but suprressable
			LEVEL_ERROR = 10, //!< A meaningful error has occurred
			LEVEL_WARN = 20,  //!< Warning of unexpected state
			LEVEL_INFO = 30,  //!< Informational logging
			LEVEL_DEBUG = 100, //!< Debug cruft
			LEVEL_DEBUG_NOISY = 200, //!< *All* the debug cruft
			LEVEL_ALL = 255,  //!< Sentinel for max value
} log_level_t;


void logline(log_level_t, const char *, ...); 

const char* log_level_to_str(log_level_t);

void log_forced(const char *, ...);

/** \} */ // End doxygen group
