#ifndef SQL_LIB_H_3341
#define SQL_LIB_H_3341

#include <cassert>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include <sqlite3.h>

class SqliteConnection;
template <typename... Args> class InStream;
template <typename... Args> class OutStream;

namespace detail {

struct SqliteFinalizer : public std::unary_function<sqlite3_stmt *, void> {
  void operator()(sqlite3_stmt *p) const { sqlite3_finalize(p); }
};

using StmtPtr = std::unique_ptr<sqlite3_stmt, SqliteFinalizer>;

} /* namespace detail */

struct sqlite_exception : std::exception {
  sqlite_exception(int sqlite_code, std::string const &operation);
  sqlite_exception(sqlite3 *db, std::string const &operation);
  explicit sqlite_exception(std::string reason);
  ~sqlite_exception() noexcept override;
  char const *what() const noexcept override;
  int code() const noexcept;

private:
  std::string reason;
  int sqlite_code;
};

template <typename... Args>
class SqliteInputIt
    : public std::iterator<std::input_iterator_tag, std::tuple<Args...>,
                           ptrdiff_t, const std::tuple<Args...> *,
                           const std::tuple<Args...> &> {

public:
  SqliteInputIt() : stream(nullptr) {}
  explicit SqliteInputIt(InStream<Args...> &s) : stream(&s) { Fetch(); }

  const std::tuple<Args...> &operator*() const;
  const std::tuple<Args...> *operator->() const;
  SqliteInputIt &operator++();
  SqliteInputIt operator++(int);
  bool operator==(const SqliteInputIt &o) const;
  bool operator!=(const SqliteInputIt &o) const;

private:
  void Fetch();

  InStream<Args...> *stream;
  std::tuple<Args...> value;
};

template <typename... Args>
class SqliteOutputIt
    : public std::iterator<std::output_iterator_tag, void, void, void, void> {
public:
  explicit SqliteOutputIt(OutStream<Args...> &s) : stream(&s) {}
  SqliteOutputIt &operator*() { return *this; }
  SqliteOutputIt &operator++() { return *this; }
  SqliteOutputIt &operator++(int) { return *this; }
  SqliteOutputIt &operator=(std::tuple<Args...> const &args);

private:
  OutStream<Args...> *stream;
};

template <typename... Args> class InStream {
public:
  std::tuple<Args...> Read();
  bool Eof() const;
  SqliteInputIt<Args...> begin();
  SqliteInputIt<Args...> end();

  InStream(const InStream &) = delete;
  InStream &operator=(const InStream &) = delete;

private:
  using StmtPtr = detail::StmtPtr;

  InStream(SqliteConnection &conn, StmtPtr &&stmt);
  void Fetch();

  SqliteConnection &conn;
  StmtPtr stmt; // will be reset() once the end of stream is reached
  // We have to buffer the next row to be able to answer if it's EOF.
  // There is no way to check other than sqlite3_step which reads the row.
  std::unique_ptr<std::tuple<Args...>> next_row;
  friend class SqliteConnection;
};

template <typename... Args> class OutStream {
public:
  void Write(const Args &... args);
  SqliteOutputIt<Args...> begin();

  OutStream(const OutStream &) = delete;
  OutStream &operator=(const OutStream &) = delete;

private:
  using StmtPtr = detail::StmtPtr;

  OutStream(SqliteConnection &conn, StmtPtr &&stmt);

  SqliteConnection &conn;
  StmtPtr stmt;
  friend class SqliteConnection;
};

template <typename... Args> class InStreamHolder {
  // The sole purpose of this class is to make SqliteConnection::Query()'s
  // result copyable, so that you can write:
  // for (const auto &row : conn.Query<int, std::string>("SELECT a, b..."))
  // This is going to get even better with structured binding in C++ 17:
  // for (const auto &[a, b] : conn.Query<int, std::string>("SELECT a, b..."))
public:
  explicit InStreamHolder(std::shared_ptr<InStream<Args...>> impl)
      : impl(std::move(std::move(impl))) {}
  InStreamHolder(const InStreamHolder<Args...> &o) : impl(o.impl) {}
  std::tuple<Args...> Read() { return this->impl->Read(); }
  bool Eof() const { return this->impl->Eof(); }
  SqliteInputIt<Args...> begin() { return this->impl->begin(); }
  SqliteInputIt<Args...> end() { return this->impl->end(); }
  InStreamHolder<Args...> &operator=(const InStreamHolder &o) {
    this->impl = o.impl;
  }

private:
  std::shared_ptr<InStream<Args...>> impl;
};

class SqliteTransaction {
public:
  explicit SqliteTransaction(SqliteConnection &conn);
  ~SqliteTransaction();
  void Rollback();
  void Commit();

private:
  SqliteConnection &conn;
  bool ongoing;
};

class SqliteConnection {
public:
  explicit SqliteConnection(std::string const &path,
                            int flags = SQLITE_OPEN_READWRITE |
                                        SQLITE_OPEN_CREATE);
  ~SqliteConnection();
  template <typename... Args>
  InStreamHolder<Args...> Query(const std::string &sql);
  template <typename... Args>
  std::unique_ptr<OutStream<Args...>> BatchInsert(const std::string &sql);
  void SqliteExec(const std::string &sql);

private:
  using StmtPtr = detail::StmtPtr;

  StmtPtr PrepareStmt(std::string const &sql);

  sqlite3 *db;

  template <typename... Args> friend class InStream;
  template <typename... Args> friend class OutStream;
};

#endif /* SQL_LIB_H_3341 */
