#include "test_common.h"

#include <sys/stat.h>

#include <boost/filesystem/fstream.hpp>
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
    // First chmod all the files so that unlink and traversal work.
    chmod(dir_.c_str(), 0777);  // ignore error codes
    for (boost::filesystem::recursive_directory_iterator dir(dir_);
         dir != boost::filesystem::recursive_directory_iterator(); ++dir) {
      chmod(dir->path().native().c_str(), 0777);  // ignore error codes
    }
    boost::filesystem::remove_all(dir_);
  } catch (const boost::filesystem::filesystem_error &e) {
    LOG(ERROR, std::string("Failed to remove temp test directory "
                           "because (") +
                   e.what() + "), leaving garbage behind (" + dir_ + ")");
  }
}

void TmpDir::CreateSubdir(const boost::filesystem::path &p) {
  boost::filesystem::create_directories(dir_ / p);
}

void TmpDir::CreateFile(const boost::filesystem::path &p,
                        const std::string &content) {
  const auto abs = dir_ / p;
  boost::filesystem::create_directories(abs.parent_path());
  boost::filesystem::ofstream f(abs);
  f << content;
}

void TmpDir::Chmod(const boost::filesystem::path &p, int perm) {
  const auto abs = dir_ / p;
  int res = chmod(abs.native().c_str(), perm);
  if (res != 0) {
    throw FsException(res, "chmod");
  }
}
