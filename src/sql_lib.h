#ifndef SQL_LIB_H_3341
#define SQL_LIB_H_3341

#include <cassert>
#include <functional>
#include <string>

#include <sqlite3.h>

#include "sql_lib_int.h"

class SqliteConnection;
template<typename... Args>
class InStream;

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
	void SqliteExec(const std::string &sql);
	void Fail(std::string const &op);

private:
	template<typename... Args>
	friend void InStream<Args...>::Fetch();
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

#endif /* SQL_LIB_H_3341 */
