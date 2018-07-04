#ifndef SQL_LIB_H_3341
#define SQL_LIB_H_3341

#include <functional>
#include <string>

#include <sqlite3.h>

#include "sql_lib_int.h"

class SqliteConnection {
public:
	typedef detail::StmtPtr StmtPtr;

	SqliteConnection(
			std::string const &path,
			int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE
			);
	~SqliteConnection();
	void StartTransaction();
	void EndTransaction();
	StmtPtr PrepareStmt(std::string const &sql);
	void SqliteExec(
			const std::string &sql,
			std::function<void(sqlite3_stmt &)> row_cb
			);
	void SqliteExec(const std::string &sql);
	void Fail(std::string const &op);

private:
	sqlite3 *db;
};

template <typename... Args>
void SqliteBind(sqlite3_stmt &s, Args... args) {
	detail::SqliteBindImpl(s, 1, args...);
}

#endif /* SQL_LIB_H_3341 */
