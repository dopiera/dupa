#ifndef SQL_LIB_H_3341
#define SQL_LIB_H_3341

#include <cassert>
#include <functional>
#include <string>

#include <sqlite3.h>

#include "sql_lib_int.h"

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



class SqliteConnection;
template<typename... Args>
class InStream;
template<typename... Args>
class OutStream;

template <typename... Args>
class SqliteInputIt : public std::iterator<
						 std::input_iterator_tag,
						 std::tuple<Args...>,
						 ptrdiff_t,
						 const std::tuple<Args...>*,
						 const std::tuple<Args...>&> {

public:
	SqliteInputIt() : stream(nullptr), ok(false) {}
	SqliteInputIt(InStream<Args...>& s) : stream(&s) { Fetch(); }

	const std::tuple<Args...>& operator*() const;
	const std::tuple<Args...>* operator->() const;
	SqliteInputIt& operator++();
	SqliteInputIt operator++(int);
	bool operator==(const SqliteInputIt& o) const;
	bool operator!=(const SqliteInputIt& o) const;

private:
	void Fetch();

	InStream<Args...> *stream;
	std::tuple<Args...> value;
	bool ok;
};

template <typename... Args>
class SqliteOutputIt : public std::iterator<
						 std::output_iterator_tag,
						 void,
						 void,
						 void,
						 void> {
public:
	SqliteOutputIt(OutStream<Args...>& s) : stream(&s) {}
	SqliteOutputIt& operator*() { return *this; }
	SqliteOutputIt& operator++() { return *this; }
	SqliteOutputIt& operator++(int) { return *this; }
	SqliteOutputIt& operator=(std::tuple<Args...> const &args);

private:
	OutStream<Args...> *stream;
};

template<typename... Args>
class InStream {
public:
	std::tuple<Args...> Read();
	bool Eof() const;
	SqliteInputIt<Args...> begin();
	SqliteInputIt<Args...> end();
private:
	typedef detail::StmtPtr StmtPtr;

	InStream(SqliteConnection &conn, StmtPtr &&stmt);
	InStream(const InStream&) = delete;
	InStream &operator=(const InStream&) = delete;
	void Fetch();

	SqliteConnection &conn;
	StmtPtr stmt;  // will be reset() once the end of stream is reached
	// We have to buffer the next row to be able to answer if it's EOF.
	// There is no way to check other than sqlite3_step which reads the row.
	std::unique_ptr<std::tuple<Args...>> next_row;
	friend class SqliteConnection;
};

template<typename... Args>
class OutStream {
public:
	void Write(const Args&... args);
private:
	typedef detail::StmtPtr StmtPtr;

	OutStream(SqliteConnection &conn, StmtPtr &&stmt);
	OutStream(const OutStream&) = delete;
	OutStream &operator=(const OutStream&) = delete;

	SqliteConnection &conn;
	StmtPtr stmt;
	friend class SqliteConnection;
};

template<typename... Args>
class InStreamHolder {
	// The sole purpose of this class is to make SqliteConnection::Query()'s
	// result copyable, so that you can write:
	// for (const auto &row : conn.Query<int, std::string>("SELECT a, b..."))
	// This is going to get even better with structured binding in C++ 17:
	// for (const auto &[a, b] : conn.Query<int, std::string>("SELECT a, b..."))
public:
	InStreamHolder(std::shared_ptr<InStream<Args...>> impl) : impl(impl) {}
	InStreamHolder(const InStreamHolder<Args...> &o) : impl(o.impl) {}
	std::tuple<Args...> Read() { return this->impl->Read(); }
	bool Eof() const { return this->impl->Eof(); }
	SqliteInputIt<Args...> begin() { return this->impl->begin(); }
	SqliteInputIt<Args...> end() { return this->impl->end(); }
	InStreamHolder<Args...> operator=(const InStreamHolder &o) {
		this->impl = o.impl;
	}
private:
	std::shared_ptr<InStream<Args...>> impl;
};

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
	InStreamHolder<Args...> Query(const std::string &sql);
	template<typename... Args>
	std::unique_ptr<OutStream<Args...>> BatchInsert(const std::string &sql);
	void SqliteExec(const std::string &sql);
	void Fail(std::string const &op);

private:
	template<typename... Args>
	friend void InStream<Args...>::Fetch();
	template<typename... Args>
	friend void OutStream<Args...>::Write(const Args&...);
	sqlite3 *db;
};

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

//======== SqliteInputIt =======================================================

// Helper for dispatching tuple's arguments to Write()
template<typename... Args, size_t... I>
inline void dispatch_impl(OutStream<Args...> &stream,
		const std::tuple<Args...>& t, index_sequence<I...>) {
	stream.Write(std::get<I>(t)...);
}


template<typename... Args>
SqliteOutputIt<Args...>& SqliteOutputIt<Args...>::operator=(
		std::tuple<Args...> const &args) {
	dispatch_impl(*this->stream, args, index_sequence_for<Args...>());
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

#endif /* SQL_LIB_H_3341 */
