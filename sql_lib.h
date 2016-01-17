#ifndef SQL_LIB_H_3341
#define SQL_LIB_H_3341

#include <functional>
#include <string>

#include <sqlite3.h>

#include "sql_lib_int.h"

struct SqliteScopedOpener {
	SqliteScopedOpener(
			std::string const &path,
			int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
	~SqliteScopedOpener();

	sqlite3 *db;
};

void StartTransaction(sqlite3 *db);
void EndTransaction(sqlite3 *db);
void SqliteExec(
		sqlite3 *db,
		const std::string &sql,
		std::function<void()> row_cb);

struct SqliteFinalizer : public std::unary_function<sqlite3_stmt*,void> {
	void operator()(sqlite3_stmt *p) const { sqlite3_finalize(p); }
};

typedef std::unique_ptr<sqlite3_stmt, SqliteFinalizer> StmtPtr;
StmtPtr PrepareStmt(sqlite3 *db, std::string const &sql);

template <typename... Args>
void SqliteBind(sqlite3_stmt &s, Args... args) {
	detail::SqliteBindImpl(s, 1, args...);
}

void SqliteExec(
		sqlite3 *db,
		const std::string &sql,
		std::function<void(sqlite3_stmt &)> row_cb
		);

#endif /* SQL_LIB_H_3341 */
