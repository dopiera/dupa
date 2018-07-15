#include "exceptions.h"

#include <cstring>

#include <string>

#include <boost/lexical_cast.hpp>

std::string fs_exception::msg_from_errno(int err,
                                         std::string const &operation) {
  size_t const buf_len = 128;
  char buf[buf_len];
  char const *const msg = strerror_r(err, buf, buf_len);
  return operation + ": " + boost::lexical_cast<std::string>(err) + " (" + msg +
         ")";
}
