#ifndef TEST_COMMON_1341
#define TEST_COMMON_1341

#include <iostream>
#include <boost/filesystem/path.hpp>

namespace boost {
namespace filesystem {

// Make boost::filesystem::path printable so that assertions have meaningful
// text.
void PrintTo(path const &p, std::ostream  *os);

} /* namespace filesystem */
} /* namespace boost */


#endif /* TEST_COMMON_1341 */
