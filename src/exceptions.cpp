#include "exceptions.h"

#include <cstring>

#include <string>

std::string FsException::MsgFromErrno(int err, std::string const &operation) {
  char buf[128];
  char const *const msg = strerror_r(err, buf, sizeof(buf));
  return operation + ": " + std::to_string(err) + " (" + msg + ")";
}
