#include "sql_lib.h"

#include "log.h"

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
		auto err_msg = detail::MakeSqliteUnique(err_msg_raw);
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


void SqliteConnection::StartTransaction() {
	char *err_msg_raw;
	int res = sqlite3_exec(this->db, "BEGIN TRANSACTION", NULL, NULL,
			&err_msg_raw);
	if (res != SQLITE_OK) {
		auto err_msg = detail::MakeSqliteUnique(err_msg_raw);
		throw sqlite_exception(this->db, "Starting transaction");
	}
}

void SqliteConnection::EndTransaction() {
	char *err_msg_raw;
	int res = sqlite3_exec(this->db, "End TRANSACTION", NULL, NULL, &err_msg_raw);
	if (res != SQLITE_OK) {
		auto err_msg = detail::MakeSqliteUnique(err_msg_raw);
		throw sqlite_exception(this->db, "Finishing transaction");
	}
}

void SqliteConnection::SqliteExec(
		const std::string &sql,
		std::function<void(sqlite3_stmt &)> row_cb
		) {
	StmtPtr stmt(this->PrepareStmt(sql));
	int res;
	while (row_cb && (res = sqlite3_step(stmt.get())) == SQLITE_ROW) {
		row_cb(*stmt);
	}
	if (res != SQLITE_DONE && res != SQLITE_OK)
		throw sqlite_exception(this->db, "Executing " + sql);
}

void SqliteConnection::SqliteExec(const std::string &sql) {
	char *err_msg_raw;
	int res = sqlite3_exec(this->db, sql.c_str(), NULL, NULL, &err_msg_raw);
	if (res != SQLITE_OK) {
		auto err_msg = detail::MakeSqliteUnique(err_msg_raw);
		throw sqlite_exception(this->db,
				std::string("Executing SQL (") + sql + "): " + err_msg.get());
	}
}

void SqliteConnection::Fail(std::string const &op) {
	throw sqlite_exception(this->db, op);
}
