#include "exceptions.h"

#include <cstring>

#include <string>

std::string FsException::MsgFromErrno(int err, const std::string &operation) {
  char buf[128];
  const char *const msg = strerror_r(err, buf, sizeof(buf));
  return operation + ": " + std::to_string(err) + " (" + msg + ")";
}
