#ifndef SRC_SCANNER_H_
#define SRC_SCANNER_H_

#include <cstdint>

#include <boost/filesystem/path.hpp>

#include "hash_cache.h" // for cksum

// DirHandle can be whatever provided that it has proper value semantics.
template <class DirHandle> struct ScanProcessor {
  virtual void File(boost::filesystem::path const &path,
                    DirHandle const &parent, file_info const &f_info) = 0;
  virtual DirHandle RootDir(boost::filesystem::path const &path) = 0;
  virtual DirHandle Dir(boost::filesystem::path const &path,
                        DirHandle const &parent) = 0;
  virtual ~ScanProcessor() = default;
};

// Will scan directory root and call appropriate methods of ScanProcessor. They
// will be called from multiple threads, but one at a time (serialized).
template <class DirHandle>
void ScanDirectory(boost::filesystem::path const &root,
                   ScanProcessor<DirHandle> &processor);

template <class DirHandle>
void ScanDb(boost::filesystem::path const &db_path,
            ScanProcessor<DirHandle> &processor);

// Will call one of the 2 above.
template <class DirHandle>
void ScanDirectoryOrDb(std::string const &path,
                       ScanProcessor<DirHandle> &processor);

#endif // SRC_SCANNER_H_
