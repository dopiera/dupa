#include "sql_lib.h"

SqliteScopedOpener::SqliteScopedOpener(std::string const &path, int flags) {
	int res = sqlite3_open_v2(path.c_str(), &this->db, flags, NULL);
	if (res != SQLITE_OK) {
		throw sqlite_exception(res, "Opening DB " + path);
	}
	// I don't care about consistency. This data is easilly regneratable.
	const char sql[] = "PRAGMA page_size = 65536; "
		"PRAGMA synchronous = 0; "
		"PRAGMA journal_mode = OFF;";
	char *err_msg_raw;
	res = sqlite3_exec(db, sql, NULL, NULL, &err_msg_raw);
	if (res != SQLITE_OK) {
		auto err_msg = detail::MakeSqliteUnique(err_msg_raw);
		throw sqlite_exception(db, std::string("Creating results tables: " ) +
				err_msg.get());
	}
}

SqliteScopedOpener::~SqliteScopedOpener() {
	int res = sqlite3_close(this->db);
	if (res != SQLITE_OK) {
		throw sqlite_exception(
				res, std::string("Clsoing DB ")
				+ sqlite3_db_filename(this->db, "main"));
	}
}

StmtPtr PrepareStmt(sqlite3 *db, std::string const &sql) {
	sqlite3_stmt *raw_stmt_ptr;
	int res = sqlite3_prepare_v2(db, sql.c_str(), -1, &raw_stmt_ptr, NULL);
	if (res != SQLITE_OK) {
	   throw sqlite_exception(db, std::string("Preparing statement: " ) + sql);
	}
	return StmtPtr(raw_stmt_ptr);
}


void StartTransaction(sqlite3 *db) {
	char *err_msg_raw;
	int res = sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, &err_msg_raw);
	if (res != SQLITE_OK) {
		auto err_msg = detail::MakeSqliteUnique(err_msg_raw);
		throw sqlite_exception(db, "Starting transaction");
	}
}

void EndTransaction(sqlite3 *db) {
	char *err_msg_raw;
	int res = sqlite3_exec(db, "End TRANSACTION", NULL, NULL, &err_msg_raw);
	if (res != SQLITE_OK) {
		auto err_msg = detail::MakeSqliteUnique(err_msg_raw);
		throw sqlite_exception(db, "Finishing transaction");
	}
}

void SqliteExec(
		sqlite3 *db,
		const std::string &sql,
		std::function<void(sqlite3_stmt &)> row_cb
		) {
	StmtPtr stmt(PrepareStmt(db, sql));
	int res;
	while ((res = sqlite3_step(stmt.get())) == SQLITE_ROW) {
		row_cb(*stmt);
	}
	if (res != SQLITE_DONE)
		throw sqlite_exception(db, "Executing " + sql);
}
