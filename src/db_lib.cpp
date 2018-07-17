#include <utility>

#include "db_lib_impl.h"

#include "log.h"

struct DBDeleter : public std::unary_function<void *, void> {
  void operator()(void *p) const { sqlite3_free(p); }
};

template <class C>
inline std::unique_ptr<C, DBDeleter> MakeDBUnique(C *o) {
  return std::unique_ptr<C, DBDeleter>(o);
}

//======== DBException =========================================================

DBException::DBException(int sqlite_code, const std::string &operation)
    : reason_(operation + ": " + sqlite3_errstr(sqlite_code)),
      sqlite_code_(sqlite_code) {}

DBException::DBException(sqlite3 *db, const std::string &operation)
    : reason_(operation + ": " + sqlite3_errmsg(db)),
      sqlite_code_(sqlite3_errcode(db)) {}

DBException::DBException(std::string reason)
    : reason_(std::move(reason)), sqlite_code_(SQLITE_ERROR) {}

DBException::~DBException() noexcept = default;

const char *DBException::what() const noexcept { return reason_.c_str(); }

int DBException::Code() const noexcept { return sqlite_code_; }

//======== DBConnection ========================================================

DBConnection::DBConnection(const std::string &path, int flags) {
  int res = sqlite3_open_v2(path.c_str(), &db_, flags, nullptr);
  if (res != SQLITE_OK) {
    throw DBException(res, "Opening DB " + path);
  }
  // I don't care about consistency. This data is easilly regneratable.
  const char sql[] =
      "PRAGMA page_size = 65536; "
      "PRAGMA synchronous = 0; "
      "PRAGMA journal_mode = OFF;"
      // Sqlite3 doesn't enforce them by default, but we will so
      // that we find bugs.
      "PRAGMA foreign_keys = 1;";
  char *err_msg_raw;
  res = sqlite3_exec(db_, sql, nullptr, nullptr, &err_msg_raw);
  if (res != SQLITE_OK) {
    auto err_msg = MakeDBUnique(err_msg_raw);
    throw DBException(db_,
                      std::string("Creating results tables: ") + err_msg.get());
  }
}

DBConnection::~DBConnection() {
  int res = sqlite3_close(db_);
  if (res != SQLITE_OK) {
    // Let's not crash the whole program and simply log it.
    LOG(ERROR, "Clsoing DB " << sqlite3_db_filename(db_, "main")
                             << "failed with code " << res << "("
                             << sqlite3_errstr(res) << "), ignoring");
  }
}

DBConnection::StmtPtr DBConnection::PrepareStmt(const std::string &sql) {
  sqlite3_stmt *raw_stmt_ptr;
  int res = sqlite3_prepare_v2(db_, sql.c_str(), -1, &raw_stmt_ptr, nullptr);
  if (res != SQLITE_OK) {
    throw DBException(db_, std::string("Preparing statement: ") + sql);
  }
  return StmtPtr(raw_stmt_ptr);
}

void DBConnection::Exec(const std::string &sql) {
  char *err_msg_raw;
  int res = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg_raw);
  if (res != SQLITE_OK) {
    auto err_msg = MakeDBUnique(err_msg_raw);
    throw DBException(
        db_, std::string("Executing SQL (") + sql + "): " + err_msg.get());
  }
}

//======== DBTransaction =======================================================

DBTransaction::DBTransaction(DBConnection &conn) : conn_(conn), ongoing_(true) {
  conn_.Exec("BEGIN TRANSACTION");
}

DBTransaction::~DBTransaction() {
  if (ongoing_) {
    // If this fails, the application is going to die, which is good because
    // if we can't roll back, we better not risk committing bad data.
    Rollback();
  }
}

void DBTransaction::Rollback() {
  conn_.Exec("ROLLBACK TRANSACTION");
  ongoing_ = false;
}

void DBTransaction::Commit() {
  conn_.Exec("COMMIT TRANSACTION");
  ongoing_ = false;
}
