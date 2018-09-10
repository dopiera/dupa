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
