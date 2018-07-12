#ifndef SQL_LIB_INT_H_7792
#define SQL_LIB_INT_H_7792

#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>

#include <sqlite3.h>

#include "sql_lib.h"

//======== Helpers =============================================================

namespace detail {

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

//======== SqliteInputIt =======================================================

template <typename... Args>
const std::tuple<Args...>& SqliteInputIt<Args...>::operator*() const {
	assert(this->ok);
	return this->value;
}

template <typename... Args>
const std::tuple<Args...>* SqliteInputIt<Args...>::operator->() const {
	return *(operator*());
}

template <typename... Args>
SqliteInputIt<Args...>& SqliteInputIt<Args...>::operator++() {
	assert(this->ok);
	Fetch();
	return *this;
}

template <typename... Args>
SqliteInputIt<Args...> SqliteInputIt<Args...>::operator++(int) {
	assert(this->ok);
	SqliteInputIt tmp = *this;
	Fetch();
	return tmp;
}

template <typename... Args>
bool SqliteInputIt<Args...>::operator==(const SqliteInputIt<Args...>& o) const {
	return (this->ok == o.ok) && (!this->ok || this->stream == o.stream);
}

template <typename... Args>
bool SqliteInputIt<Args...>::operator!=(const SqliteInputIt<Args...>& o) const {
	return !(*this == o);
}

template <typename... Args>
void SqliteInputIt<Args...>::Fetch() {
	this->ok = this->stream && !this->stream->Eof();
	if (this->ok) {
		this->value = stream->Read();
		this->ok = !this->stream->Eof();
	}
}

//======== SqliteOutputIt ======================================================

// Helper for dispatching tuple's arguments to Write()
template<typename... Args, size_t... I>
inline void dispatch_impl(OutStream<Args...> &stream,
		const std::tuple<Args...>& t, detail::index_sequence<I...>) {
	stream.Write(std::get<I>(t)...);
}


template<typename... Args>
SqliteOutputIt<Args...>& SqliteOutputIt<Args...>::operator=(
		std::tuple<Args...> const &args) {
	dispatch_impl(*this->stream, args, detail::index_sequence_for<Args...>());
	return *this;
}

//======== Misc ================================================================

template <typename... Args>
void SqliteBind(sqlite3_stmt &s, Args... args) {
	detail::SqliteBindImpl(s, 1, args...);
}

//======== SqliteConnection ====================================================

template<typename... Args>
InStreamHolder<Args...> SqliteConnection::Query(
		const std::string &sql) {
	auto in_stream = std::shared_ptr<InStream<Args...>>(
				new InStream<Args...>(*this, this->PrepareStmt(sql))
			);
	return InStreamHolder<Args...>(in_stream);
}

template<typename... Args>
std::unique_ptr<OutStream<Args...>> SqliteConnection::BatchInsert(
		const std::string &sql) {
	return std::unique_ptr<OutStream<Args...>>(
			new OutStream<Args...>(*this, this->PrepareStmt(sql)));
}

//======== InStream ============================================================

template<typename... Args>
void InStream<Args...>::Fetch() {
	this->next_row.reset();
	const int res = sqlite3_step(stmt.get());
	switch (res) {
		case SQLITE_DONE:
			this->stmt.reset();
			break;
		case SQLITE_ROW:
			this->next_row.reset(new std::tuple<Args...>(
						detail::Extract<Args...>(*this->conn.db, *stmt)));
			break;
			// Consider special handling of SQLITE_OK to indicate misuse
		default:
			throw sqlite_exception(res, "Trying to read from stream.");
	}
}

template<typename... Args>
InStream<Args...>::InStream(
		SqliteConnection &conn, StmtPtr &&stmt) :
		conn(conn),
		stmt(std::move(stmt)) {
	Fetch();
}

template<typename... Args>
std::tuple<Args...> InStream<Args...>::Read() {
	std::unique_ptr<std::tuple<Args...>> res(std::move(this->next_row));
	Fetch();
	return *res;
}

template<typename... Args>
bool InStream<Args...>::Eof() const {
	return !this->next_row;
}

template<typename... Args>
SqliteInputIt<Args...> InStream<Args...>::begin() {
	return SqliteInputIt<Args...>(*this);
}

template<typename... Args>
SqliteInputIt<Args...> InStream<Args...>::end() {
	return SqliteInputIt<Args...>();
}

//======== OutStream ===========================================================

template<typename... Args>
OutStream<Args...>::OutStream(SqliteConnection &conn, detail::StmtPtr &&stmt)
	: conn(conn), stmt(std::move(stmt)) {
}

template<typename... Args>
void OutStream<Args...>::Write(const Args&... args) {
	SqliteBind(*this->stmt, args...);
	int res = sqlite3_step(this->stmt.get());
	if (res != SQLITE_DONE)
		throw sqlite_exception(this->conn.db, "Advancing output stream");
	res = sqlite3_clear_bindings(this->stmt.get());
	if (res != SQLITE_OK)
		throw sqlite_exception(this->conn.db,
				"Clearing output stream bindings");
	res = sqlite3_reset(this->stmt.get());
	if (res != SQLITE_OK)
		throw sqlite_exception(this->conn.db,
				"Resetting statement in output stream");
}


#endif /* SQL_LIB_INT_H_7792 */
