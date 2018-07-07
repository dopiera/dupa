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
	template<typename... Args>
	void SqliteExec(
			const std::string &sql,
			std::function<void(std::tuple<Args...> const&)> row_cb
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

template<typename... Args>
void SqliteConnection::SqliteExec(
		const std::string &sql,
		std::function<void(std::tuple<Args...> const&)> row_cb
		) {
	StmtPtr stmt(this->PrepareStmt(sql));
	int res;
	while (row_cb && (res = sqlite3_step(stmt.get())) == SQLITE_ROW) {
		row_cb(detail::Extract<Args...>(*this->db, *stmt));
	}
	if (res != SQLITE_DONE && res != SQLITE_OK)
		throw sqlite_exception(this->db, "Executing " + sql);
}


#endif /* SQL_LIB_H_3341 */
