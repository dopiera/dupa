#ifndef SRC_EXCEPTIONS_H_
#define SRC_EXCEPTIONS_H_

#include <exception>
#include <string>

struct FsException : std::exception {
  FsException(int err, std::string const &operation)
      : msg_(FsException::MsgFromErrno(err, operation)) {}
  ~FsException() noexcept override = default;
  char const *what() const noexcept override { return msg_.c_str(); }

private:
 static std::string MsgFromErrno(int err, std::string const &operation);

 std::string msg_;
};

#endif // SRC_EXCEPTIONS_H_
