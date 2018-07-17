#ifndef SRC_SCANNER_H_
#define SRC_SCANNER_H_

#include <cstdint>

#include <boost/filesystem/path.hpp>

#include "hash_cache.h"  // for cksum

// DIR_HANDLE can be whatever provided that it has proper value semantics.
template <class DIR_HANDLE>
class ScanProcessor {
 public:
  virtual void File(const boost::filesystem::path &path,
                    const DIR_HANDLE &parent, const FileInfo &f_info) = 0;
  virtual DIR_HANDLE RootDir(const boost::filesystem::path &path) = 0;
  virtual DIR_HANDLE Dir(const boost::filesystem::path &path,
                         const DIR_HANDLE &parent) = 0;
  virtual ~ScanProcessor() = default;
};

// Will scan directory root and call appropriate methods of ScanProcessor. They
// will be called from multiple threads, but one at a time (serialized).
template <class DIR_HANDLE>
void ScanDirectory(const boost::filesystem::path &root,
                   ScanProcessor<DIR_HANDLE> &processor);

template <class DIR_HANDLE>
void ScanDb(const boost::filesystem::path &db_path,
            ScanProcessor<DIR_HANDLE> &processor);

// Will call one of the 2 above.
template <class DIR_HANDLE>
void ScanDirectoryOrDb(const std::string &path,
                       ScanProcessor<DIR_HANDLE> &processor);

#endif  // SRC_SCANNER_H_
