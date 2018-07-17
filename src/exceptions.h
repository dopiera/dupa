#ifndef SRC_EXCEPTIONS_H_
#define SRC_EXCEPTIONS_H_

#include <exception>
#include <string>

struct FsException : std::exception {
  FsException(int err, const std::string &operation)
      : msg_(FsException::MsgFromErrno(err, operation)) {}
  ~FsException() noexcept override = default;
  const char *what() const noexcept override { return msg_.c_str(); }

 private:
  static std::string MsgFromErrno(int err, const std::string &operation);

  std::string msg_;
};

#endif  // SRC_EXCEPTIONS_H_
