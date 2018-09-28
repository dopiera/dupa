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

#include "scanner_int.h"

#include <iterator>

namespace detail {

using boost::filesystem::path;

path CommonPathPrefix(const path &p1, const path &p2) {
  path res;
  for (path::const_iterator p1i = p1.begin(), p2i = p2.begin();
       p1i != p1.end() && p2i != p2.end() && *p1i == *p2i; ++p1i, ++p2i) {
    res = res.empty() ? *p1i : (res / *p1i);
  }
  return res;
}

}  // namespace detail
