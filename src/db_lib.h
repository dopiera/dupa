#ifndef SRC_DB_LIB_H_
#define SRC_DB_LIB_H_

#include <cassert>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include <sqlite3.h>

class DBConnection;
template <typename... ARGS>
class DBInStream;
template <typename... ARGS>
class DBOutStream;

namespace detail {

struct SqliteFinalizer : public std::unary_function<sqlite3_stmt *, void> {
  void operator()(sqlite3_stmt *p) const { sqlite3_finalize(p); }
};

using DBStmtPtr = std::unique_ptr<sqlite3_stmt, SqliteFinalizer>;

} /* namespace detail */

class DBException : public std::exception {
 public:
  DBException(int sqlite_code, const std::string &operation);
  DBException(sqlite3 *db, const std::string &operation);
  explicit DBException(std::string reason);
  ~DBException() noexcept override;
  const char *what() const noexcept override;
  int Code() const noexcept;

 private:
  std::string reason_;
  int sqlite_code_;
};

template <typename... ARGS>
class DBInputIt
    : public std::iterator<std::input_iterator_tag, std::tuple<ARGS...>,
                           ptrdiff_t, const std::tuple<ARGS...> *,
                           const std::tuple<ARGS...> &> {
 public:
  DBInputIt() : stream_(nullptr) {}
  explicit DBInputIt(DBInStream<ARGS...> &s) : stream_(&s) { Fetch(); }

  const std::tuple<ARGS...> &operator*() const;
  const std::tuple<ARGS...> *operator->() const;
  DBInputIt &operator++();
  DBInputIt operator++(int);
  bool operator==(const DBInputIt &o) const;
  bool operator!=(const DBInputIt &o) const;

 private:
  void Fetch();

  DBInStream<ARGS...> *stream_;
  std::tuple<ARGS...> value_;
};

template <typename... ARGS>
class DBOutputIt
    : public std::iterator<std::output_iterator_tag, void, void, void, void> {
 public:
  explicit DBOutputIt(DBOutStream<ARGS...> &s) : stream_(&s) {}
  DBOutputIt &operator*() { return *this; }
  DBOutputIt &operator++() { return *this; }
  DBOutputIt &operator++(int) { return *this; }
  DBOutputIt &operator=(const std::tuple<ARGS...> &args);

 private:
  DBOutStream<ARGS...> *stream_;
};

template <typename... ARGS>
class DBInStream {
 public:
  std::tuple<ARGS...> Read();
  bool Eof() const;
  DBInputIt<ARGS...> begin();  // NOLINT
  DBInputIt<ARGS...> end();    // NOLINT

  DBInStream(const DBInStream &) = delete;
  DBInStream &operator=(const DBInStream &) = delete;

 private:
  using StmtPtr = detail::DBStmtPtr;

  DBInStream(DBConnection &conn, StmtPtr &&stmt);
  void Fetch();

  DBConnection &conn_;
  StmtPtr stmt_;  // will be reset() once the end of stream is reached
  // We have to buffer the next row to be able to answer if it's EOF.
  // There is no way to check other than sqlite3_step which reads the row.
  std::unique_ptr<std::tuple<ARGS...>> next_row_;
  friend class DBConnection;
};

template <typename... ARGS>
class DBOutStream {
 public:
  void Write(const ARGS &... args);
  DBOutputIt<ARGS...> begin();  // NOLINT

  DBOutStream(const DBOutStream &) = delete;
  DBOutStream &operator=(const DBOutStream &) = delete;

 private:
  using StmtPtr = detail::DBStmtPtr;

  DBOutStream(DBConnection &conn, StmtPtr &&stmt);

  DBConnection &conn_;
  StmtPtr stmt_;
  friend class DBConnection;
};

template <typename... ARGS>
class DBInStreamPtr {
  // The sole purpose of this class is to make DBConnection::Query()'s
  // result copyable, so that you can write:
  // for (const auto &[a, b] : conn.Query<int, std::string>("SELECT a, b..."))
 public:
  explicit DBInStreamPtr(std::shared_ptr<DBInStream<ARGS...>> impl)
      : impl_(std::move(std::move(impl))) {}
  DBInStreamPtr(const DBInStreamPtr<ARGS...> &o) : impl_(o.impl) {}
  std::tuple<ARGS...> Read() { return impl_->Read(); }
  bool Eof() const { return impl_->Eof(); }
  DBInputIt<ARGS...> begin() { return impl_->begin(); }  // NOLINT
  DBInputIt<ARGS...> end() { return impl_->end(); }      // NOLINT
  DBInStreamPtr<ARGS...> &operator=(const DBInStreamPtr &o) { impl_ = o.impl_; }

 private:
  std::shared_ptr<DBInStream<ARGS...>> impl_;
};

class DBTransaction {
 public:
  explicit DBTransaction(DBConnection &conn);
  ~DBTransaction();
  void Rollback();
  void Commit();

 private:
  DBConnection &conn_;
  bool ongoing_;
};

class DBConnection {
 public:
  explicit DBConnection(const std::string &path,
                        int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
  ~DBConnection();
  template <typename... ARGS>
  DBInStreamPtr<ARGS...> Query(const std::string &sql);
  template <typename... ARGS>
  std::unique_ptr<DBOutStream<ARGS...>> Prepare(const std::string &sql);
  void Exec(const std::string &sql);

 private:
  using StmtPtr = detail::DBStmtPtr;

  StmtPtr PrepareStmt(const std::string &sql);

  sqlite3 *db_;

  template <typename... ARGS>
  friend class DBInStream;
  template <typename... ARGS>
  friend class DBOutStream;
};

#endif  // SRC_DB_LIB_H_
