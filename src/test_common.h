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

#endif  // SRC_TEST_COMMON_H_
