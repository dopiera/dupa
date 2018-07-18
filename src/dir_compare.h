#ifndef SRC_DIR_COMPARE_H_
#define SRC_DIR_COMPARE_H_

#include <boost/filesystem/path.hpp>

void WarmupCache(const boost::filesystem::path &path);
void DirCompare(const boost::filesystem::path &dir1,
                const boost::filesystem::path &dir2);

#endif /* SRC_DIR_COMPARE_H_ */
