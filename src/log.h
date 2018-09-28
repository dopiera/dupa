/*
 * (C) Copyright 2018 Marek Dopiera
 *
 * This file is part of dupa.
 *
 * dupa is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * dupa is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with dupa. If not, see http://www.gnu.org/licenses/.
 */

#ifndef SRC_LOG_H_
#define SRC_LOG_H_

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

#define LOG(level, message)                              \
  do {                                                   \
    if ((level) >= stderr_loglevel) {                    \
      ::std::cerr << __FILE__ << ":" << __LINE__ << ": " \
                  << message /* NOLINT */ << std::endl;  \
    }                                                    \
  } while (0)

#ifndef NDEBUG
#define DLOG(message) LOG(DEBUG, message)
#else
#define DLOG(message) \
  do {                \
  } while (0)
#endif /* NDEBUG */

#endif  // SRC_LOG_H_
