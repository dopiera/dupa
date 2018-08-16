#ifndef SRC_DIR_COMPARE_H_
#define SRC_DIR_COMPARE_H_

#include <boost/filesystem/path.hpp>

void WarmupCache(const boost::filesystem::path &path);
void DirCompare(const std::string &dir1, const std::string &dir2);

#endif /* SRC_DIR_COMPARE_H_ */
