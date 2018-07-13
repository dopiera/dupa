#include "test_common.h"

namespace boost {
namespace filesystem {

void PrintTo(path const &p, std::ostream *os) { (*os) << p.native(); }

} /* namespace filesystem */
} /* namespace boost */
