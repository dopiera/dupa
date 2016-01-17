#ifndef SQL_LIB_H_3341
#define SQL_LIB_H_3341

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

struct SqliteFinalizer : public std::unary_function<sqlite3_stmt*,void> {
	void operator()(sqlite3_stmt *p) const { sqlite3_finalize(p); }
};

template <class C>
inline std::unique_ptr<C, detail::SqliteDeleter> MakeSqliteUnique(C *o) {
	return std::unique_ptr<C, detail::SqliteDeleter>(o);
}

typedef std::unique_ptr<sqlite3_stmt, SqliteFinalizer> StmtPtr;
StmtPtr PrepareStmt(sqlite3 *db, std::string const &sql);

template <typename... Args>
void SqliteBind(sqlite3_stmt &s, Args... args) {
	detail::SqliteBindImpl(s, 1, args...);
}

#endif /* SQL_LIB_H_3341 */
