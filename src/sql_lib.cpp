#include <utility>

#include "sql_lib_impl.h"

#include "log.h"

struct SqliteDeleter : public std::unary_function<void *, void> {
  void operator()(void *p) const { sqlite3_free(p); }
};

template <class C>
inline std::unique_ptr<C, SqliteDeleter> MakeSqliteUnique(C *o) {
  return std::unique_ptr<C, SqliteDeleter>(o);
}

//======== SqliteException =====================================================

SqliteException::SqliteException(int sqlite_code, std::string const &operation)
    : reason_(operation + ": " + sqlite3_errstr(sqlite_code)),
      sqlite_code_(sqlite_code) {}

SqliteException::SqliteException(sqlite3 *db, std::string const &operation)
    : reason_(operation + ": " + sqlite3_errmsg(db)),
      sqlite_code_(sqlite3_errcode(db)) {}

SqliteException::SqliteException(std::string reason)
    : reason_(std::move(reason)), sqlite_code_(SQLITE_ERROR) {}

SqliteException::~SqliteException() noexcept = default;

char const *SqliteException::what() const noexcept {
  return this->reason_.c_str();
}

int SqliteException::Code() const noexcept { return this->sqlite_code_; }

//======== SqliteConnection ====================================================

SqliteConnection::SqliteConnection(std::string const &path, int flags) {
  int res = sqlite3_open_v2(path.c_str(), &this->db_, flags, nullptr);
  if (res != SQLITE_OK) {
    throw SqliteException(res, "Opening DB " + path);
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
  res = sqlite3_exec(this->db_, sql, nullptr, nullptr, &err_msg_raw);
  if (res != SQLITE_OK) {
    auto err_msg = MakeSqliteUnique(err_msg_raw);
    throw SqliteException(
        this->db_, std::string("Creating results tables: ") + err_msg.get());
  }
}

SqliteConnection::~SqliteConnection() {
  int res = sqlite3_close(this->db_);
  if (res != SQLITE_OK) {
    // Let's not crash the whole program and simply log it.
    LOG(ERROR, "Clsoing DB " << sqlite3_db_filename(this->db_, "main")
                             << "failed with code " << res << "("
                             << sqlite3_errstr(res) << "), ignoring");
  }
}

SqliteConnection::StmtPtr SqliteConnection::PrepareStmt(
    std::string const &sql) {
  sqlite3_stmt *raw_stmt_ptr;
  int res =
      sqlite3_prepare_v2(this->db_, sql.c_str(), -1, &raw_stmt_ptr, nullptr);
  if (res != SQLITE_OK) {
    throw SqliteException(this->db_,
                          std::string("Preparing statement: ") + sql);
  }
  return StmtPtr(raw_stmt_ptr);
}

void SqliteConnection::SqliteExec(const std::string &sql) {
  char *err_msg_raw;
  int res =
      sqlite3_exec(this->db_, sql.c_str(), nullptr, nullptr, &err_msg_raw);
  if (res != SQLITE_OK) {
    auto err_msg = MakeSqliteUnique(err_msg_raw);
    throw SqliteException(this->db_, std::string("Executing SQL (") + sql +
                                         "): " + err_msg.get());
  }
}

//======== SqliteTransaction ===================================================

SqliteTransaction::SqliteTransaction(SqliteConnection &conn)
    : conn_(conn), ongoing_(true) {
  this->conn_.SqliteExec("BEGIN TRANSACTION");
}

SqliteTransaction::~SqliteTransaction() {
  if (this->ongoing_) {
    // If this fails, the application is going to die, which is good because
    // if we can't roll back, we better not risk committing bad data.
    this->Rollback();
  }
}

void SqliteTransaction::Rollback() {
  this->conn_.SqliteExec("ROLLBACK TRANSACTION");
  this->ongoing_ = false;
}

void SqliteTransaction::Commit() {
  this->conn_.SqliteExec("COMMIT TRANSACTION");
  this->ongoing_ = false;
}
