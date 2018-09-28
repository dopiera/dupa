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

#ifndef SRC_TEST_COMMON_H_
#define SRC_TEST_COMMON_H_

#include <boost/filesystem/path.hpp>
#include <iostream>

namespace boost {
namespace filesystem {

// Make boost::filesystem::path printable so that assertions have meaningful
// text.
void PrintTo(const path &p, std::ostream *os);

} /* namespace filesystem */
} /* namespace boost */

class TmpDir {
 public:
  TmpDir();
  ~TmpDir();
  // Create a hierarchy od subdirectories inside this dir.
  void CreateSubdir(const boost::filesystem::path &p);
  // Create a file under a subdirectory of this dir. Intermediate dirs are
  // created automatically.
  void CreateFile(const boost::filesystem::path &p,
                  const std::string &content = std::string());
  void Chmod(const boost::filesystem::path &p, int perm);

  std::string dir_;
};

#endif  // SRC_TEST_COMMON_H_
