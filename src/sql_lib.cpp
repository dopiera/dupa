#include "sql_lib_impl.h"

#include "log.h"

struct SqliteDeleter : public std::unary_function<void*,void> {
	void operator()(void *p) const { sqlite3_free(p); }
};

template <class C>
inline std::unique_ptr<C, SqliteDeleter> MakeSqliteUnique(C *o) {
	return std::unique_ptr<C, SqliteDeleter>(o);
}

//======== sqlite_exception ====================================================

sqlite_exception::sqlite_exception(
		int sqlite_code, std::string const &operation) :
		reason(operation + ": " + sqlite3_errstr(sqlite_code)) {
}

sqlite_exception::sqlite_exception(sqlite3 *db, std::string const &operation) :
		reason(operation + ": " + sqlite3_errmsg(db)) {}

sqlite_exception::sqlite_exception(std::string const &reason) : reason(reason) {}

sqlite_exception::~sqlite_exception() throw() {}

char const * sqlite_exception::what() const throw() {
	return this->reason.c_str();
}

//======== SqliteConnection ====================================================

SqliteConnection::SqliteConnection(std::string const &path, int flags) {
	int res = sqlite3_open_v2(path.c_str(), &this->db, flags, NULL);
	if (res != SQLITE_OK) {
		throw sqlite_exception(res, "Opening DB " + path);
	}
	// I don't care about consistency. This data is easilly regneratable.
	const char sql[] = "PRAGMA page_size = 65536; "
		"PRAGMA synchronous = 0; "
		"PRAGMA journal_mode = OFF;"
		"PRAGMA foreign_keys = 1;";  // one can never be too sure
	char *err_msg_raw;
	res = sqlite3_exec(this->db, sql, NULL, NULL, &err_msg_raw);
	if (res != SQLITE_OK) {
		auto err_msg = MakeSqliteUnique(err_msg_raw);
		throw sqlite_exception(this->db,
				std::string("Creating results tables: " ) + err_msg.get());
	}
}

SqliteConnection::~SqliteConnection() {
	int res = sqlite3_close(this->db);
	if (res != SQLITE_OK) {
		// Let's not crash the whole program and simply log it.
		LOG(ERROR, "Clsoing DB " << sqlite3_db_filename(this->db, "main") <<
				"failed with code " << res << "(" << sqlite3_errstr(res) <<
				"), ignoring");
	}
}

SqliteConnection::StmtPtr SqliteConnection::PrepareStmt(std::string const &sql)
{
	sqlite3_stmt *raw_stmt_ptr;
	int res = sqlite3_prepare_v2(this->db, sql.c_str(), -1, &raw_stmt_ptr,
			NULL);
	if (res != SQLITE_OK) {
	   throw sqlite_exception(this->db, std::string("Preparing statement: " ) +
			   sql);
	}
	return StmtPtr(raw_stmt_ptr);
}


void SqliteConnection::SqliteExec(const std::string &sql) {
	char *err_msg_raw;
	int res = sqlite3_exec(this->db, sql.c_str(), NULL, NULL, &err_msg_raw);
	if (res != SQLITE_OK) {
		auto err_msg = MakeSqliteUnique(err_msg_raw);
		throw sqlite_exception(this->db,
				std::string("Executing SQL (") + sql + "): " + err_msg.get());
	}
}

//======== SqliteTransaction ===================================================

SqliteTransaction::SqliteTransaction(SqliteConnection &conn)
	: conn(conn), ongoing(true) {
	this->conn.SqliteExec("BEGIN TRANSACTION");
}

SqliteTransaction::~SqliteTransaction() {
	if (this->ongoing) {
		// If this fails, the application is going to die, which is good because
		// if we can't roll back, we better not risk committing bad data.
		this->Rollback();
	}
}

void SqliteTransaction::Rollback() {
	this->conn.SqliteExec("ROLLBACK TRANSACTION");
	this->ongoing = false;
}

void SqliteTransaction::Commit() {
	this->conn.SqliteExec("COMMIT TRANSACTION");
	this->ongoing = false;
}
