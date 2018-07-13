#include "scanner_int.h"

#include <iterator>

namespace detail {

using boost::filesystem::path;

path common_path_prefix(path const &p1, path const &p2) {
  path res;
  for (path::const_iterator p1i = p1.begin(), p2i = p2.begin();
       p1i != p1.end() && p2i != p2.end() && *p1i == *p2i; ++p1i, ++p2i) {
    res = res.empty() ? *p1i : (res / *p1i);
  }
  return res;
}

} // namespace detail
