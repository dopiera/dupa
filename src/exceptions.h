#ifndef SRC_EXCEPTIONS_H_
#define SRC_EXCEPTIONS_H_

#include <exception>
#include <string>

struct fs_exception : std::exception {
  fs_exception(int err, std::string const &operation)
      : msg(fs_exception::msg_from_errno(err, operation)) {}
  ~fs_exception() noexcept override = default;
  char const *what() const noexcept override { return msg.c_str(); }

private:
  static std::string msg_from_errno(int err, std::string const &operation);

  std::string msg;
};

#endif // SRC_EXCEPTIONS_H_
