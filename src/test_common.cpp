#include "test_common.h"

#include <boost/filesystem/operations.hpp>

#include "exceptions.h"
#include "log.h"

namespace boost {
namespace filesystem {

void PrintTo(const path &p, std::ostream *os) { (*os) << p.native(); }

} /* namespace filesystem */
} /* namespace boost */

TmpDir::TmpDir() {
  char tmp_dir[] = "/tmp/dupa.XXXXXX";
  if (!mkdtemp(tmp_dir)) {
    throw FsException(errno, "Creating a temp directory");
  }
  dir_ = tmp_dir;
}

TmpDir::~TmpDir() {
  try {
    boost::filesystem::remove_all(dir_);
  } catch (const boost::filesystem::filesystem_error &e) {
    LOG(ERROR, std::string("Failed to remove temp test directory "
                           "because (") +
                   e.what() + "), leaving garbage behind (" + dir_ + ")");
  }
}
