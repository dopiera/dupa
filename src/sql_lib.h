#ifndef SRC_SQL_LIB_H_
#define SRC_SQL_LIB_H_

#include <cassert>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include <sqlite3.h>

class SqliteConnection;
template <typename... ARGS>
class InStream;
template <typename... ARGS>
class OutStream;

namespace detail {

struct SqliteFinalizer : public std::unary_function<sqlite3_stmt *, void> {
  void operator()(sqlite3_stmt *p) const { sqlite3_finalize(p); }
};

using StmtPtr = std::unique_ptr<sqlite3_stmt, SqliteFinalizer>;

} /* namespace detail */

struct SqliteException : std::exception {
  SqliteException(int sqlite_code, std::string const &operation);
  SqliteException(sqlite3 *db, std::string const &operation);
  explicit SqliteException(std::string reason);
  ~SqliteException() noexcept override;
  char const *what() const noexcept override;
  int Code() const noexcept;

 private:
  std::string reason_;
  int sqlite_code_;
};

template <typename... ARGS>
class SqliteInputIt
    : public std::iterator<std::input_iterator_tag, std::tuple<ARGS...>,
                           ptrdiff_t, const std::tuple<ARGS...> *,
                           const std::tuple<ARGS...> &> {
 public:
  SqliteInputIt() : stream_(nullptr) {}
  explicit SqliteInputIt(InStream<ARGS...> &s) : stream_(&s) { Fetch(); }

  const std::tuple<ARGS...> &operator*() const;
  const std::tuple<ARGS...> *operator->() const;
  SqliteInputIt &operator++();
  SqliteInputIt operator++(int);
  bool operator==(const SqliteInputIt &o) const;
  bool operator!=(const SqliteInputIt &o) const;

 private:
  void Fetch();

  InStream<ARGS...> *stream_;
  std::tuple<ARGS...> value_;
};

template <typename... ARGS>
class SqliteOutputIt
    : public std::iterator<std::output_iterator_tag, void, void, void, void> {
 public:
  explicit SqliteOutputIt(OutStream<ARGS...> &s) : stream_(&s) {}
  SqliteOutputIt &operator*() { return *this; }
  SqliteOutputIt &operator++() { return *this; }
  SqliteOutputIt &operator++(int) { return *this; }
  SqliteOutputIt &operator=(std::tuple<ARGS...> const &args);

 private:
  OutStream<ARGS...> *stream_;
};

template <typename... ARGS>
class InStream {
 public:
  std::tuple<ARGS...> Read();
  bool Eof() const;
  SqliteInputIt<ARGS...> begin();  // NOLINT
  SqliteInputIt<ARGS...> end();    // NOLINT

  InStream(const InStream &) = delete;
  InStream &operator=(const InStream &) = delete;

 private:
  using StmtPtr = detail::StmtPtr;

  InStream(SqliteConnection &conn, StmtPtr &&stmt);
  void Fetch();

  SqliteConnection &conn_;
  StmtPtr stmt_;  // will be reset() once the end of stream is reached
  // We have to buffer the next row to be able to answer if it's EOF.
  // There is no way to check other than sqlite3_step which reads the row.
  std::unique_ptr<std::tuple<ARGS...>> next_row_;
  friend class SqliteConnection;
};

template <typename... ARGS>
class OutStream {
 public:
  void Write(const ARGS &... args);
  SqliteOutputIt<ARGS...> begin();  // NOLINT

  OutStream(const OutStream &) = delete;
  OutStream &operator=(const OutStream &) = delete;

 private:
  using StmtPtr = detail::StmtPtr;

  OutStream(SqliteConnection &conn, StmtPtr &&stmt);

  SqliteConnection &conn_;
  StmtPtr stmt_;
  friend class SqliteConnection;
};

template <typename... ARGS>
class InStreamHolder {
  // The sole purpose of this class is to make SqliteConnection::Query()'s
  // result copyable, so that you can write:
  // for (const auto &[a, b] : conn.Query<int, std::string>("SELECT a, b..."))
 public:
  explicit InStreamHolder(std::shared_ptr<InStream<ARGS...>> impl)
      : impl_(std::move(std::move(impl))) {}
  InStreamHolder(const InStreamHolder<ARGS...> &o) : impl_(o.impl) {}
  std::tuple<ARGS...> Read() { return this->impl_->Read(); }
  bool Eof() const { return this->impl_->Eof(); }
  SqliteInputIt<ARGS...> begin() { return this->impl_->begin(); }  // NOLINT
  SqliteInputIt<ARGS...> end() { return this->impl_->end(); }      // NOLINT
  InStreamHolder<ARGS...> &operator=(const InStreamHolder &o) {
    this->impl_ = o.impl_;
  }

 private:
  std::shared_ptr<InStream<ARGS...>> impl_;
};

class SqliteTransaction {
 public:
  explicit SqliteTransaction(SqliteConnection &conn);
  ~SqliteTransaction();
  void Rollback();
  void Commit();

 private:
  SqliteConnection &conn_;
  bool ongoing_;
};

class SqliteConnection {
 public:
  explicit SqliteConnection(std::string const &path,
                            int flags = SQLITE_OPEN_READWRITE |
                                        SQLITE_OPEN_CREATE);
  ~SqliteConnection();
  template <typename... ARGS>
  InStreamHolder<ARGS...> Query(const std::string &sql);
  template <typename... ARGS>
  std::unique_ptr<OutStream<ARGS...>> BatchInsert(const std::string &sql);
  void SqliteExec(const std::string &sql);

 private:
  using StmtPtr = detail::StmtPtr;

  StmtPtr PrepareStmt(std::string const &sql);

  sqlite3 *db_;

  template <typename... ARGS>
  friend class InStream;
  template <typename... ARGS>
  friend class OutStream;
};

#endif  // SRC_SQL_LIB_H_
