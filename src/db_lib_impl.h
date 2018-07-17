#ifndef SRC_DB_LIB_IMPL_H_
#define SRC_DB_LIB_IMPL_H_

#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include <sqlite3.h>

#include "db_lib.h"

//======== Helpers =============================================================

namespace detail {

template <typename C, class ENABLED = void>
struct Bind1 {
  static_assert(sizeof(C) == -1, "Don't know how to bind this type.");
  int operator()(sqlite3_stmt &s, int idx, const C &value);
};

template <typename C, typename ENABLED = void>
struct Unpack1 {
  static_assert(sizeof(C) == -1, "Don't know how to unpack this type.");
  C operator()(sqlite3_stmt &s, int idx);
};

template <typename C>
struct Bind1<C, typename std::enable_if<std::is_integral<C>::value>::type> {
  int operator()(sqlite3_stmt &s, int idx, const C &value) {
    return sqlite3_bind_int64(&s, idx, value);
  }
};

template <typename C>
struct Unpack1<C, typename std::enable_if<std::is_integral<C>::value>::type> {
  C operator()(sqlite3_stmt &row, int idx) {
    return sqlite3_column_int64(&row, idx);
  }
};

template <typename C>
struct Bind1<C,
             typename std::enable_if<std::is_floating_point<C>::value>::type> {
  int operator()(sqlite3_stmt &s, int idx, const C &value) {
    return sqlite3_bind_double(&s, idx, value);
  }
};

template <typename C>
struct Unpack1<
    C, typename std::enable_if<std::is_floating_point<C>::value>::type> {
  C operator()(sqlite3_stmt &row, int idx) {
    return sqlite3_column_double(&row, idx);
  }
};

template <>
struct Bind1<std::string> {
  int operator()(sqlite3_stmt &s, int idx, const std::string &str) {
    auto *mem = static_cast<char *>(malloc(str.length() + 1));
    if (mem == nullptr) {
      throw std::bad_alloc();
    }

    strncpy(mem, str.c_str(), str.length() + 1);
    int res = sqlite3_bind_text(&s, idx, mem, str.length(), free);
    if (res != SQLITE_OK) {
      free(mem);
    }
    return res;
  }
};

template <>
struct Unpack1<std::string> {
  std::string operator()(sqlite3_stmt &row, int idx) {
    return std::string(
        reinterpret_cast<const char *>(sqlite3_column_text(&row, idx)));
  }
};

inline void BindImpl(sqlite3_stmt & /*s*/, int /*idx*/) {}

template <typename T, typename... ARGS>
inline void BindImpl(sqlite3_stmt &s, int idx, const T &a, ARGS... args) {
  int res = Bind1<T>()(s, idx, a);
  if (res != SQLITE_OK) {
    throw DBException(res, "Binding parameter " + std::to_string(idx));
  }
  BindImpl(s, idx + 1, args...);
}

template <typename... ARGS>
void Bind(sqlite3_stmt &s, ARGS... args) {
  detail::BindImpl(s, 1, args...);
}

template <typename... ARGS>
struct UnpackImpl;

template <>
struct UnpackImpl<> {
  inline std::tuple<> operator()(sqlite3 & /*db*/, sqlite3_stmt & /*row*/,
                                 int /*idx*/) const {
    return std::tuple<>();
  }
};

template <typename T, typename... ARGS>
struct UnpackImpl<T, ARGS...> {
  inline std::tuple<T, ARGS...> operator()(sqlite3 &db, sqlite3_stmt &row,
                                           int idx) const {
    const T &t = Unpack1<T>()(row, idx);
    const int err = sqlite3_errcode(&db);
    if (err == SQLITE_NOMEM) {
      // This is an sqlite3 weirdness. Unpacking values will only fail if
      // there is a memory allocation failure. What's worse, otherwise,
      // the errcode is not set at all, so we need to compare to this
      // specific error code only.
      throw DBException(&db, std::string("Unpacking value (") +
                                 std::to_string(idx) + ") from result");
    }
    return std::tuple_cat(std::tuple<T>(t),
                          UnpackImpl<ARGS...>()(db, row, idx + 1));
  }
};

template <typename... ARGS>
std::tuple<ARGS...> Unpack(sqlite3 &db, sqlite3_stmt &row) {
  return UnpackImpl<ARGS...>()(db, row, 0);
}

} /* namespace detail */

//======== DBInputIt ===========================================================

template <typename... ARGS>
const std::tuple<ARGS...> &DBInputIt<ARGS...>::operator*() const {
  return value_;
}

template <typename... ARGS>
const std::tuple<ARGS...> *DBInputIt<ARGS...>::operator->() const {
  return *(operator*());
}

template <typename... ARGS>
DBInputIt<ARGS...> &DBInputIt<ARGS...>::operator++() {
  assert(stream_);
  Fetch();
  return *this;
}

template <typename... ARGS>
DBInputIt<ARGS...> DBInputIt<ARGS...>::operator++(int) {
  assert(stream_);
  DBInputIt tmp = *this;
  Fetch();
  return tmp;
}

template <typename... ARGS>
bool DBInputIt<ARGS...>::operator==(const DBInputIt<ARGS...> &o) const {
  if (stream_) {
    return stream_ == o.stream_;
  }
  return !o.stream_;
}

template <typename... ARGS>
bool DBInputIt<ARGS...>::operator!=(const DBInputIt<ARGS...> &o) const {
  return !(*this == o);
}

template <typename... ARGS>
void DBInputIt<ARGS...>::Fetch() {
  if (stream_) {
    if (stream_->Eof()) {
      stream_ = nullptr;
    } else {
      value_ = stream_->Read();
    }
  }
}

//======== DBOutputIt ==========================================================

// Helper for dispatching tuple's arguments to Write()
template <typename... ARGS, size_t... I>
inline void DispatchImpl(DBOutStream<ARGS...> &stream,
                         const std::tuple<ARGS...> &t,
                         std::index_sequence<I...> /*unused*/) {
  stream.Write(std::get<I>(t)...);
}

template <typename... ARGS>
DBOutputIt<ARGS...> &DBOutputIt<ARGS...>::operator=(
    const std::tuple<ARGS...> &args) {
  DispatchImpl(*stream_, args, std::index_sequence_for<ARGS...>());
  return *this;
}

//======== DBConnection ========================================================

template <typename... ARGS>
DBInStreamPtr<ARGS...> DBConnection::Query(const std::string &sql) {
  auto in_stream = std::shared_ptr<DBInStream<ARGS...>>(
      new DBInStream<ARGS...>(*this, PrepareStmt(sql)));
  return DBInStreamPtr<ARGS...>(in_stream);
}

template <typename... ARGS>
std::unique_ptr<DBOutStream<ARGS...>> DBConnection::Prepare(
    const std::string &sql) {
  return std::unique_ptr<DBOutStream<ARGS...>>(
      new DBOutStream<ARGS...>(*this, PrepareStmt(sql)));
}

//======== DBInStream ==========================================================

template <typename... ARGS>
void DBInStream<ARGS...>::Fetch() {
  next_row_.reset();
  const int res = sqlite3_step(stmt_.get());
  switch (res) {
    case SQLITE_DONE:
      stmt_.reset();
      break;
    case SQLITE_ROW:
      next_row_.reset(
          new std::tuple<ARGS...>(detail::Unpack<ARGS...>(*conn_.db_, *stmt_)));
      break;
      // Consider special handling of SQLITE_OK to indicate misuse
    default:
      throw DBException(res, "Trying to read from stream.");
  }
}

template <typename... ARGS>
DBInStream<ARGS...>::DBInStream(DBConnection &conn, StmtPtr &&stmt)
    : conn_(conn), stmt_(std::move(stmt)) {
  Fetch();
}

template <typename... ARGS>
std::tuple<ARGS...> DBInStream<ARGS...>::Read() {
  std::unique_ptr<std::tuple<ARGS...>> res(std::move(next_row_));
  Fetch();
  return *res;
}

template <typename... ARGS>
bool DBInStream<ARGS...>::Eof() const {
  return !next_row_;
}

template <typename... ARGS>
DBInputIt<ARGS...> DBInStream<ARGS...>::begin() {  // NOLINT
  return DBInputIt<ARGS...>(*this);
}

template <typename... ARGS>
DBInputIt<ARGS...> DBInStream<ARGS...>::end() {  // NOLINT
  return DBInputIt<ARGS...>();
}

//======== DBOutStream =========================================================

template <typename... ARGS>
DBOutStream<ARGS...>::DBOutStream(DBConnection &conn, detail::DBStmtPtr &&stmt)
    : conn_(conn), stmt_(std::move(stmt)) {}

template <typename... ARGS>
void DBOutStream<ARGS...>::Write(const ARGS &... args) {
  detail::Bind(*stmt_, args...);
  int res = sqlite3_step(stmt_.get());
  if (res != SQLITE_DONE) {
    throw DBException(conn_.db_, "Advancing output stream");
  }
  res = sqlite3_clear_bindings(stmt_.get());
  if (res != SQLITE_OK) {
    throw DBException(conn_.db_, "Clearing output stream bindings");
  }
  res = sqlite3_reset(stmt_.get());
  if (res != SQLITE_OK) {
    throw DBException(conn_.db_, "Resetting statement in output stream");
  }
}

template <typename... ARGS>
DBOutputIt<ARGS...> DBOutStream<ARGS...>::begin() {  // NOLINT
  return DBOutputIt<ARGS...>(*this);
}

#endif  // SRC_DB_LIB_IMPL_H_
