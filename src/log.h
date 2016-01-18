#ifndef LOG_H_64352
#define LOG_H_64352

#include <cassert>
#include <iostream>

enum LogLevel {
	DEBUG = 1,
	INFO = 2,
	WARNING = 3,
	ERROR = 4,
	FATAL = 5,
};

extern LogLevel stderr_loglevel;

#define LOG(level, message) \
	do { \
		if ((level) >= stderr_loglevel) { \
		   ::std::cerr << __FILE__ << ":" << __LINE__ \
				<< ": " << message << std::endl; \
		} \
	} while (0)

#ifndef NDEBUG
#define DLOG(message) LOG(DEBUG, message)
#else
#define DLOG(message) do {} while (0)
#endif /* NDEBUG */

#endif /* LOG_H_64352 */
