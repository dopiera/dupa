#include "sql_lib.h"

SqliteScopedOpener::SqliteScopedOpener(std::string const &path, int flags) {
	int res = sqlite3_open_v2(path.c_str(), &this->db, flags, NULL);
	if (res != SQLITE_OK) {
		throw sqlite_exception(res, "Opening DB " + path);
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
		auto err_msg = MakeSqliteUnique(err_msg_raw);
		throw sqlite_exception(db, "Starting transaction");
	}
}

void EndTransaction(sqlite3 *db) {
	char *err_msg_raw;
	int res = sqlite3_exec(db, "End TRANSACTION", NULL, NULL, &err_msg_raw);
	if (res != SQLITE_OK) {
		auto err_msg = MakeSqliteUnique(err_msg_raw);
		throw sqlite_exception(db, "Finishing transaction");
	}
}

