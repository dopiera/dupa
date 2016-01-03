#include "db_output.h"

#include <new>

#include "sqlite3.h"

SqliteScopedOpener::SqliteScopedOpener(std::string const &path, int flags) {
	int res = sqlite3_open_v2(path.c_str(), &this->db, flags, NULL);
	if (res != SQLITE_OK) {
		throw sqlite_exception(res, "Opening DB " + path);
	}
}

SqliteScopedOpener::~SqliteScopedOpener() {
	int res = sqlite3_close(this->db);
	if (res != SQLITE_OK) {
		throw sqlite_exception(
				res, std::string("Clsoing DB ")
				+ sqlite3_db_filename(this->db, "main"));
	}
}

void CreateResultsDatabase(sqlite3 *db) {
   char const sql[] =
	   "DROP TABLE IF EXISTS Node;"
	   "DROP TABLE IF EXISTS EqClass;"
	   "DROP TABLE IF EXISTS EqClassDup;"
	   "CREATE TABLE EqClass("
	     "id INT PRIMARY KEY     NOT NULL,"
		 "nodes          INT     NOT NULL,"
		 "weight         DOUBLE);"
	   "CREATE TABLE EqClassDup("
	     "id INT PRIMARY KEY     NOT NULL,"
		 "nodes          INT     NOT NULL,"
		 "weight         DOUBLE);"
	   "CREATE TABLE Node("
         "id INT PRIMARY KEY     NOT NULL,"
         "name           TEXT    NOT NULL,"
         "path           TEXT    NOT NULL,"
         "type           CHAR(5) NOT NULL,"
         "cksum          INTEGER,"  // NULL for directories
         "eq_class		 INT     NOT NULL);";
   char *err_msg_raw;
   int res = sqlite3_exec(db, sql, NULL, NULL, &err_msg_raw);
   if (res != SQLITE_OK) {
	   auto err_msg = MakeSqliteUnique(err_msg_raw);
	   throw sqlite_exception(db, std::string("Creating results tables: " ) +
			   err_msg.get());
   }
}

typedef std::unique_ptr<sqlite3_stmt, SqliteFinalizer> StmtPtr;
static StmtPtr PrepareStmt(sqlite3 *db, std::string const &sql) {
	sqlite3_stmt *raw_stmt_ptr;
	int res = sqlite3_prepare_v2(db, sql.c_str(), -1, &raw_stmt_ptr, NULL);
	if (res != SQLITE_OK) {
	   throw sqlite_exception(db, std::string("Preparing statement: " ) + sql);
	}
	return StmtPtr(raw_stmt_ptr);
}

template <class C>
void SqliteBind1(sqlite3_stmt &s, int idx, C const & value);

template <>
void SqliteBind1<sqlite3_int64>(sqlite3_stmt &s,
		int idx, sqlite3_int64 const &value) {
	int res = sqlite3_bind_int64(&s, idx, value);
	if (res != SQLITE_OK)
	   throw sqlite_exception(res, "Binding parameter.");
}

template <>
void SqliteBind1<double>(sqlite3_stmt &s, int idx,
		double const &value) {
	int res = sqlite3_bind_double(&s, idx, value);
	if (res != SQLITE_OK)
	   throw sqlite_exception(res, "Binding parameter.");
}

template <>
void SqliteBind1<uintptr_t>(sqlite3_stmt &s, int idx, uintptr_t const &value) {
	SqliteBind1<sqlite3_int64>(s, idx, value);
}

template <>
void SqliteBind1<std::string>(sqlite3_stmt &s, int idx,
		std::string const & str) {
	char *mem = static_cast<char*>(malloc(str.length() + 1));
	if (mem == NULL)
		throw std::bad_alloc();

	strcpy(mem, str.c_str());
	int res = sqlite3_bind_text(&s, idx, mem, str.length(), free);
	if (res != SQLITE_OK)
	   throw sqlite_exception(res, "Binding parameter.");
}

void SqliteBindImpl(sqlite3_stmt &s, int idx) {}

template <typename T, typename... Args>
void SqliteBindImpl(sqlite3_stmt &s, int idx, T const &a, Args... args) {
	SqliteBind1(s, idx, a);
	SqliteBindImpl(s, idx + 1, args...);
}


template <typename... Args>
void SqliteBind(sqlite3_stmt &s, Args... args) {
	SqliteBindImpl(s, 1, args...);
}

static void StartTransaction(sqlite3 *db) {
	char *err_msg_raw;
	int res = sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, &err_msg_raw);
	if (res != SQLITE_OK) {
		auto err_msg = MakeSqliteUnique(err_msg_raw);
		throw sqlite_exception(db, "Starting transaction");
	}
}

static void EndTransaction(sqlite3 *db) {
	char *err_msg_raw;
	int res = sqlite3_exec(db, "End TRANSACTION", NULL, NULL, &err_msg_raw);
	if (res != SQLITE_OK) {
		auto err_msg = MakeSqliteUnique(err_msg_raw);
		throw sqlite_exception(db, "Finishing transaction");
	}
}

void DumpInterestingEqClasses(sqlite3 *db,
		std::vector<EqClass*> const &eq_classes) {
	char const eq_class_sql[] =
		"INSERT INTO EqClassDup(id, nodes, weight) VALUES(?, ?, ?)";
	StmtPtr eq_class_stmt(PrepareStmt(db, eq_class_sql));
	StartTransaction(db);
	for (EqClass const *eq_class: eq_classes) {
		SqliteBind(
				*eq_class_stmt,
				reinterpret_cast<uintptr_t>(eq_class),
				eq_class->GetNumNodes(),
				eq_class->GetWeight()
				);
		int res = sqlite3_step(eq_class_stmt.get());
		if (res != SQLITE_DONE)
			throw sqlite_exception(db, "Inserting EqClass");
		res = sqlite3_clear_bindings(eq_class_stmt.get());
		if (res != SQLITE_OK)
		   throw sqlite_exception(db, "Clearing EqClass bindings");
		res = sqlite3_reset(eq_class_stmt.get());
		if (res != SQLITE_OK)
		   throw sqlite_exception(db, "Clearing EqClass bindings");
	}
	EndTransaction(db);
}

void DumpFuzzyDedupRes(sqlite3 *db, FuzzyDedupRes const &res) {
	char const eq_class_sql[] =
		"INSERT INTO EqClass(id, nodes, weight) VALUES(?, ?, ?)";
	char const node_sql[] =
		"INSERT INTO Node(id, name, path, type, eq_class) "
			"VALUES(?, ?, ?, ?, ?)";
	StmtPtr eq_class_stmt(PrepareStmt(db, eq_class_sql));
	StmtPtr node_stmt(PrepareStmt(db, node_sql));
	StartTransaction(db);
	for (EqClass const &eq_class: *res.second) {
		SqliteBind(
				*eq_class_stmt,
				reinterpret_cast<uintptr_t>(&eq_class),
				eq_class.GetNumNodes(),
				eq_class.GetWeight()
				);
		int res = sqlite3_step(eq_class_stmt.get());
		if (res != SQLITE_DONE)
			throw sqlite_exception(db, "Inserting EqClass");
		res = sqlite3_clear_bindings(eq_class_stmt.get());
		if (res != SQLITE_OK)
		   throw sqlite_exception(db, "Clearing EqClass bindings");
		res = sqlite3_reset(eq_class_stmt.get());
		if (res != SQLITE_OK)
		   throw sqlite_exception(db, "Clearing EqClass bindings");
	}
	res.first->Traverse([&] (Node const *n) {
		SqliteBind(
				*node_stmt,
				reinterpret_cast<uintptr_t>(n),
				n->GetName(),
				n->BuildPath().native(),
				std::string((n->GetType() == Node::FILE) ? "FILE" : "DIR"),
				reinterpret_cast<uintptr_t>(&n->GetEqClass())
				);
		int res = sqlite3_step(node_stmt.get());
		if (res != SQLITE_DONE)
		   throw sqlite_exception(db, "Inserting EqClass");
		res = sqlite3_clear_bindings(node_stmt.get());
		if (res != SQLITE_OK)
		   throw sqlite_exception(db, "Clearing EqClass bindings");
		res = sqlite3_reset(node_stmt.get());
		if (res != SQLITE_OK)
		   throw sqlite_exception(db, "Clearing EqClass bindings");
	});
	EndTransaction(db);
}
