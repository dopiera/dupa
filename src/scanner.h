#ifndef SRC_SCANNER_H_
#define SRC_SCANNER_H_

#include <cstdint>

#include <boost/filesystem/path.hpp>

#include "hash_cache.h"  // for cksum

// DIR_HANDLE can be whatever provided that it has proper value semantics.
template <class DIR_HANDLE>
struct ScanProcessor {
  virtual void File(boost::filesystem::path const &path,
                    DIR_HANDLE const &parent, FileInfo const &f_info) = 0;
  virtual DIR_HANDLE RootDir(boost::filesystem::path const &path) = 0;
  virtual DIR_HANDLE Dir(boost::filesystem::path const &path,
                         DIR_HANDLE const &parent) = 0;
  virtual ~ScanProcessor() = default;
};

// Will scan directory root and call appropriate methods of ScanProcessor. They
// will be called from multiple threads, but one at a time (serialized).
template <class DIR_HANDLE>
void ScanDirectory(boost::filesystem::path const &root,
                   ScanProcessor<DIR_HANDLE> &processor);

template <class DIR_HANDLE>
void ScanDb(boost::filesystem::path const &db_path,
            ScanProcessor<DIR_HANDLE> &processor);

// Will call one of the 2 above.
template <class DIR_HANDLE>
void ScanDirectoryOrDb(std::string const &path,
                       ScanProcessor<DIR_HANDLE> &processor);

#endif  // SRC_SCANNER_H_
