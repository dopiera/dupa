#include "log.h"

#ifndef NDEBUG
LogLevel stderr_loglevel = DEBUG;
#else
LogLevel stderr_loglevel = WARNING;
#endif /* NDEBUG */
