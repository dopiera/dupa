#ifndef DB_OUTPUT_H_12662
#define DB_OUTPUT_H_12662

#include <exception>
#include <memory>
#include <string>

#include <sqlite3.h>

#include "fuzzy_dedup.h"

struct sqlite_exception : std::exception {
	sqlite_exception(int sqlite_code, std::string const &operation) :
		reason(operation + ": " + sqlite3_errstr(sqlite_code)) {}
	sqlite_exception(sqlite3 *db, std::string const &operation) :
		reason(operation + ": " + sqlite3_errmsg(db)) {}
	sqlite_exception(std::string const &reason) : reason(reason) {}
	~sqlite_exception() throw() {}
	virtual char const * what() const throw() { return this->reason.c_str(); }
private:
	std::string reason;
};

struct SqliteScopedOpener {
	SqliteScopedOpener(
			std::string const &path,
			int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
	~SqliteScopedOpener();

	sqlite3 *db;
};

struct SqliteDeleter : public std::unary_function<void*,void> {
	void operator()(void *p) const { sqlite3_free(p); } 
};

struct SqliteFinalizer : public std::unary_function<sqlite3_stmt*,void> {
	void operator()(sqlite3_stmt *p) const { sqlite3_finalize(p); } 
};

template <class C>
inline std::unique_ptr<C, SqliteDeleter> MakeSqliteUnique(C *o) {
	return std::unique_ptr<C, SqliteDeleter>(o);
}

void CreateResultsDatabase(sqlite3 *db);
void DumpFuzzyDedupRes(sqlite3 *db, FuzzyDedupRes const &res);
void DumpInterestingEqClasses(sqlite3 *db,
		std::vector<EqClass*> const &eq_classes);

#endif /* DB_OUTPUT_H_12662 */
