#ifndef SQL_LIB_INT_H_7792
#define SQL_LIB_INT_H_7792

#include <cstdint>
#include <cstring>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <tuple>

#include <sqlite3.h>

struct sqlite_exception : std::exception {
	sqlite_exception(int sqlite_code, std::string const &operation);
	sqlite_exception(sqlite3 *db, std::string const &operation);
	sqlite_exception(std::string const &reason);
	~sqlite_exception() throw();
	virtual char const * what() const throw();
private:
	std::string reason;
};

namespace detail {

struct SqliteFinalizer : public std::unary_function<sqlite3_stmt*,void> {
	void operator()(sqlite3_stmt *p) const { sqlite3_finalize(p); }
};
struct SqliteDeleter : public std::unary_function<void*,void> {
	void operator()(void *p) const { sqlite3_free(p); }
};

typedef std::unique_ptr<sqlite3_stmt, SqliteFinalizer> StmtPtr;

template <typename C, class Enabled = void>
struct SqliteBind1
{
	static_assert(sizeof(C) == -1, "Don't know how to bind this type.");
	void operator()(sqlite3_stmt &s, int idx, C const & value);
};

template <typename C>
struct SqliteBind1<C, typename std::enable_if<std::is_integral<C>::value>::type>
{
	void operator()(sqlite3_stmt &s, int idx, C const & value) {
		int res = sqlite3_bind_int64(&s, idx, value);
		if (res != SQLITE_OK)
		   throw sqlite_exception(res, "Binding parameter.");
	}
};

template <typename C>
struct SqliteBind1<C, typename std::enable_if<std::is_floating_point<C>::value>::type>
{
	void operator()(sqlite3_stmt &s, int idx, C const & value) {
		int res = sqlite3_bind_double(&s, idx, value);
		if (res != SQLITE_OK)
		   throw sqlite_exception(res, "Binding parameter.");
	}
};

template <>
struct SqliteBind1<std::string>
{
	void operator()(sqlite3_stmt &s, int idx, std::string const & str) {
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
};

inline void SqliteBindImpl(sqlite3_stmt &s, int idx) {}

template <typename T, typename... Args>
inline void SqliteBindImpl(sqlite3_stmt &s, int idx, T const &a, Args... args) {
	SqliteBind1<T>()(s, idx, a);
	SqliteBindImpl(s, idx + 1, args...);
}

template <typename C, typename Enabled = void>
struct ExtractCell
{
	static_assert(sizeof(C) == -1, "Don't know how to extract this type.");
	C operator()(sqlite3_stmt &s, int idx);
};

template <typename C>
struct ExtractCell<C, typename std::enable_if<std::is_integral<C>::value>::type>
{
	C operator()(sqlite3_stmt &row, int idx) {
		return sqlite3_column_int64(&row, idx);
	}
};

template <typename C>
struct ExtractCell<C, typename std::enable_if<std::is_floating_point<C>::value>::type>
{
	C operator()(sqlite3_stmt &row, int idx) {
		return sqlite3_column_double(&row, idx);
	}
};

template <>
struct ExtractCell<std::string>
{
	std::string operator()(sqlite3_stmt &row, int idx) {
		return std::string(reinterpret_cast<const char*>(
					sqlite3_column_text(&row, idx)));
	}
};

template <typename... Args>
struct ExtractImpl;

template<>
struct ExtractImpl<>
{
	inline std::tuple<> operator()(
			sqlite3 &db, sqlite3_stmt &row, int idx) const {
		return std::tuple<>();
	}
};

template<typename T, typename... Args>
struct ExtractImpl<T, Args...>
{
	inline std::tuple<T, Args...> operator()(
			sqlite3 &db, sqlite3_stmt &row, int idx) const {
		const T &t = ExtractCell<T>()(row, idx);
		const int err = sqlite3_errcode(&db);
		if (err == SQLITE_NOMEM) {
			// This is an sqlite3 weirdness. Extracting values will only fail if
			// there is a memory allocation failure. What's worse, otherwise,
			// the errcode is not set at all, so we need to compare to this
			// specific error code only.
			throw sqlite_exception(&db, std::string("Extracting value (") +
					std::to_string(idx) + ") from result");
		}
		return std::tuple_cat(
				std::tuple<T>(t),
				ExtractImpl<Args...>()(db, row, idx + 1));
	}
};

template<typename... Args>
std::tuple<Args...> Extract(sqlite3 &db, sqlite3_stmt &row) {
	return ExtractImpl<Args...>()(db, row, 0);
}

// Reimplement C++14 index_sequence_for

template <size_t... I> struct index_sequence {
        typedef size_t value_type;
        static constexpr size_t size() { return sizeof...(I); }
};

template <std::size_t N, std::size_t... I>
struct build_index_impl : build_index_impl<N - 1, N - 1, I...> {};
template <std::size_t... I>
struct build_index_impl<0, I...> : index_sequence<I...> {};

template <class... Ts>
struct index_sequence_for : build_index_impl<sizeof...(Ts)> {};

} /* namespace detail */


#endif /* SQL_LIB_INT_H_7792 */
