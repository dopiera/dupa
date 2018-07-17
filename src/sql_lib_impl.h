#ifndef SRC_SQL_LIB_IMPL_H_
#define SRC_SQL_LIB_IMPL_H_

#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include <sqlite3.h>

#include "sql_lib.h"

//======== Helpers =============================================================

namespace detail {

template <typename C, class ENABLED = void>
struct SqliteBind1 {
  static_assert(sizeof(C) == -1, "Don't know how to bind this type.");
  void operator()(sqlite3_stmt &s, int idx, const C &value);
};

template <typename C>
struct SqliteBind1<C,
                   typename std::enable_if<std::is_integral<C>::value>::type> {
  void operator()(sqlite3_stmt &s, int idx, const C &value) {
    int res = sqlite3_bind_int64(&s, idx, value);
    if (res != SQLITE_OK) {
      throw SqliteException(res, "Binding parameter.");
    }
  }
};

template <typename C>
struct SqliteBind1<
    C, typename std::enable_if<std::is_floating_point<C>::value>::type> {
  void operator()(sqlite3_stmt &s, int idx, const C &value) {
    int res = sqlite3_bind_double(&s, idx, value);
    if (res != SQLITE_OK) {
      throw SqliteException(res, "Binding parameter.");
    }
  }
};

template <>
struct SqliteBind1<std::string> {
  void operator()(sqlite3_stmt &s, int idx, const std::string &str) {
    auto *mem = static_cast<char *>(malloc(str.length() + 1));
    if (mem == nullptr) {
      throw std::bad_alloc();
    }

    strncpy(mem, str.c_str(), str.length() + 1);
    int res = sqlite3_bind_text(&s, idx, mem, str.length(), free);
    if (res != SQLITE_OK) {
      free(mem);
      throw SqliteException(res, "Binding parameter.");
    }
  }
};

inline void SqliteBindImpl(sqlite3_stmt & /*s*/, int /*idx*/) {}

template <typename T, typename... ARGS>
inline void SqliteBindImpl(sqlite3_stmt &s, int idx, const T &a, ARGS... args) {
  SqliteBind1<T>()(s, idx, a);
  SqliteBindImpl(s, idx + 1, args...);
}

template <typename C, typename ENABLED = void>
struct ExtractCell {
  static_assert(sizeof(C) == -1, "Don't know how to extract this type.");
  C operator()(sqlite3_stmt &s, int idx);
};

template <typename C>
struct ExtractCell<C,
                   typename std::enable_if<std::is_integral<C>::value>::type> {
  C operator()(sqlite3_stmt &row, int idx) {
    return sqlite3_column_int64(&row, idx);
  }
};

template <typename C>
struct ExtractCell<
    C, typename std::enable_if<std::is_floating_point<C>::value>::type> {
  C operator()(sqlite3_stmt &row, int idx) {
    return sqlite3_column_double(&row, idx);
  }
};

template <>
struct ExtractCell<std::string> {
  std::string operator()(sqlite3_stmt &row, int idx) {
    return std::string(
        reinterpret_cast<const char *>(sqlite3_column_text(&row, idx)));
  }
};

template <typename... ARGS>
struct ExtractImpl;

template <>
struct ExtractImpl<> {
  inline std::tuple<> operator()(sqlite3 & /*db*/, sqlite3_stmt & /*row*/,
                                 int /*idx*/) const {
    return std::tuple<>();
  }
};

template <typename T, typename... ARGS>
struct ExtractImpl<T, ARGS...> {
  inline std::tuple<T, ARGS...> operator()(sqlite3 &db, sqlite3_stmt &row,
                                           int idx) const {
    const T &t = ExtractCell<T>()(row, idx);
    const int err = sqlite3_errcode(&db);
    if (err == SQLITE_NOMEM) {
      // This is an sqlite3 weirdness. Extracting values will only fail if
      // there is a memory allocation failure. What's worse, otherwise,
      // the errcode is not set at all, so we need to compare to this
      // specific error code only.
      throw SqliteException(&db, std::string("Extracting value (") +
                                     std::to_string(idx) + ") from result");
    }
    return std::tuple_cat(std::tuple<T>(t),
                          ExtractImpl<ARGS...>()(db, row, idx + 1));
  }
};

template <typename... ARGS>
std::tuple<ARGS...> Extract(sqlite3 &db, sqlite3_stmt &row) {
  return ExtractImpl<ARGS...>()(db, row, 0);
}

} /* namespace detail */

//======== SqliteInputIt =======================================================

template <typename... ARGS>
const std::tuple<ARGS...> &SqliteInputIt<ARGS...>::operator*() const {
  return value_;
}

template <typename... ARGS>
const std::tuple<ARGS...> *SqliteInputIt<ARGS...>::operator->() const {
  return *(operator*());
}

template <typename... ARGS>
SqliteInputIt<ARGS...> &SqliteInputIt<ARGS...>::operator++() {
  assert(stream_);
  Fetch();
  return *this;
}

template <typename... ARGS>
SqliteInputIt<ARGS...> SqliteInputIt<ARGS...>::operator++(int) {
  assert(stream_);
  SqliteInputIt tmp = *this;
  Fetch();
  return tmp;
}

template <typename... ARGS>
bool SqliteInputIt<ARGS...>::operator==(const SqliteInputIt<ARGS...> &o) const {
  if (stream_) {
    return stream_ == o.stream_;
  }
  return !o.stream_;
}

template <typename... ARGS>
bool SqliteInputIt<ARGS...>::operator!=(const SqliteInputIt<ARGS...> &o) const {
  return !(*this == o);
}

template <typename... ARGS>
void SqliteInputIt<ARGS...>::Fetch() {
  if (stream_) {
    if (stream_->Eof()) {
      stream_ = nullptr;
    } else {
      value_ = stream_->Read();
    }
  }
}

//======== SqliteOutputIt ======================================================

// Helper for dispatching tuple's arguments to Write()
template <typename... ARGS, size_t... I>
inline void DispatchImpl(OutStream<ARGS...> &stream,
                         const std::tuple<ARGS...> &t,
                         std::index_sequence<I...> /*unused*/) {
  stream.Write(std::get<I>(t)...);
}

template <typename... ARGS>
SqliteOutputIt<ARGS...> &SqliteOutputIt<ARGS...>::operator=(
    const std::tuple<ARGS...> &args) {
  DispatchImpl(*stream_, args, std::index_sequence_for<ARGS...>());
  return *this;
}

//======== Misc ================================================================

template <typename... ARGS>
void SqliteBind(sqlite3_stmt &s, ARGS... args) {
  detail::SqliteBindImpl(s, 1, args...);
}

//======== SqliteConnection ====================================================

template <typename... ARGS>
InStreamHolder<ARGS...> SqliteConnection::Query(const std::string &sql) {
  auto in_stream = std::shared_ptr<InStream<ARGS...>>(
      new InStream<ARGS...>(*this, PrepareStmt(sql)));
  return InStreamHolder<ARGS...>(in_stream);
}

template <typename... ARGS>
std::unique_ptr<OutStream<ARGS...>> SqliteConnection::BatchInsert(
    const std::string &sql) {
  return std::unique_ptr<OutStream<ARGS...>>(
      new OutStream<ARGS...>(*this, PrepareStmt(sql)));
}

//======== InStream ============================================================

template <typename... ARGS>
void InStream<ARGS...>::Fetch() {
  next_row_.reset();
  const int res = sqlite3_step(stmt_.get());
  switch (res) {
    case SQLITE_DONE:
      stmt_.reset();
      break;
    case SQLITE_ROW:
      next_row_.reset(new std::tuple<ARGS...>(
          detail::Extract<ARGS...>(*conn_.db_, *stmt_)));
      break;
      // Consider special handling of SQLITE_OK to indicate misuse
    default:
      throw SqliteException(res, "Trying to read from stream.");
  }
}

template <typename... ARGS>
InStream<ARGS...>::InStream(SqliteConnection &conn, StmtPtr &&stmt)
    : conn_(conn), stmt_(std::move(stmt)) {
  Fetch();
}

template <typename... ARGS>
std::tuple<ARGS...> InStream<ARGS...>::Read() {
  std::unique_ptr<std::tuple<ARGS...>> res(std::move(next_row_));
  Fetch();
  return *res;
}

template <typename... ARGS>
bool InStream<ARGS...>::Eof() const {
  return !next_row_;
}

template <typename... ARGS>
SqliteInputIt<ARGS...> InStream<ARGS...>::begin() {  // NOLINT
  return SqliteInputIt<ARGS...>(*this);
}

template <typename... ARGS>
SqliteInputIt<ARGS...> InStream<ARGS...>::end() {  // NOLINT
  return SqliteInputIt<ARGS...>();
}

//======== OutStream ===========================================================

template <typename... ARGS>
OutStream<ARGS...>::OutStream(SqliteConnection &conn, detail::StmtPtr &&stmt)
    : conn_(conn), stmt_(std::move(stmt)) {}

template <typename... ARGS>
void OutStream<ARGS...>::Write(const ARGS &... args) {
  SqliteBind(*stmt_, args...);
  int res = sqlite3_step(stmt_.get());
  if (res != SQLITE_DONE) {
    throw SqliteException(conn_.db_, "Advancing output stream");
  }
  res = sqlite3_clear_bindings(stmt_.get());
  if (res != SQLITE_OK) {
    throw SqliteException(conn_.db_, "Clearing output stream bindings");
  }
  res = sqlite3_reset(stmt_.get());
  if (res != SQLITE_OK) {
    throw SqliteException(conn_.db_, "Resetting statement in output stream");
  }
}

template <typename... ARGS>
SqliteOutputIt<ARGS...> OutStream<ARGS...>::begin() {  // NOLINT
  return SqliteOutputIt<ARGS...>(*this);
}

#endif  // SRC_SQL_LIB_IMPL_H_
