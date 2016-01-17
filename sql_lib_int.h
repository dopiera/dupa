#ifndef SQL_LIB_INT_H_7792
#define SQL_LIB_INT_H_7792

#include <cstdint>
#include <cstring>
#include <exception>
#include <functional>
#include <memory>
#include <string>

#include <sqlite3.h>

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

namespace detail {

struct SqliteDeleter : public std::unary_function<void*,void> {
	void operator()(void *p) const { sqlite3_free(p); }
};

template <class C>
void SqliteBind1(sqlite3_stmt &s, int idx, C const & value);

template <>
inline void SqliteBind1<sqlite3_int64>(sqlite3_stmt &s,
		int idx, sqlite3_int64 const &value) {
	int res = sqlite3_bind_int64(&s, idx, value);
	if (res != SQLITE_OK)
	   throw sqlite_exception(res, "Binding parameter.");
}

template <>
inline void SqliteBind1<double>(sqlite3_stmt &s, int idx,
		double const &value) {
	int res = sqlite3_bind_double(&s, idx, value);
	if (res != SQLITE_OK)
	   throw sqlite_exception(res, "Binding parameter.");
}

template <>
inline void SqliteBind1<uintptr_t>(sqlite3_stmt &s, int idx,
		uintptr_t const &value) {
	SqliteBind1<sqlite3_int64>(s, idx, value);
}

template <>
inline void SqliteBind1<std::string>(sqlite3_stmt &s, int idx,
		std::string const & str) {
	char *mem = static_cast<char*>(malloc(str.length() + 1));
	if (mem == NULL)
		throw std::bad_alloc();

	strcpy(mem, str.c_str());
	int res = sqlite3_bind_text(&s, idx, mem, str.length(), free);
	if (res != SQLITE_OK) {
		free(mem);
		throw sqlite_exception(res, "Binding parameter.");
	}
}

inline void SqliteBindImpl(sqlite3_stmt &s, int idx) {}

template <typename T, typename... Args>
inline void SqliteBindImpl(sqlite3_stmt &s, int idx, T const &a, Args... args) {
	SqliteBind1(s, idx, a);
	SqliteBindImpl(s, idx + 1, args...);
}

} /* namespace detail */

#endif /* SQL_LIB_INT_H_7792 */
