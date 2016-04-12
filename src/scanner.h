#ifndef SCANNER_H_6513
#define SCANNER_H_6513

#include <stdint.h>

#include <boost/filesystem/path.hpp>

#include "hash_cache.h" // for cksum

// DirHandle can be whatever provided that it has proper value semantics.
template <class DirHandle>
struct ScanProcessor {
	virtual void File(
			boost::filesystem::path const &path,
			DirHandle const &parent,
			cksum cksum) = 0;
	virtual DirHandle RootDir(boost::filesystem::path const &path) = 0;
	virtual DirHandle Dir(
			boost::filesystem::path const &path,
			DirHandle const &parent
			) = 0;
	virtual ~ScanProcessor() {}
};

// Will scan directory root and call appropriate methods of ScanProcessor. They
// will be called from multiple threads, but one at a time (serialized).
template <class DirHandle>
void ScanDirectory(
		boost::filesystem::path const &root,
		ScanProcessor<DirHandle> &processor);

#endif /* SCANNER_H_6513 */
