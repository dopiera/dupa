#include "db_output.h"

#include <new>

#include "sql_lib.h"

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
         "eq_class		 INT     NOT NULL,"
	    "FOREIGN KEY(eq_class) REFERENCES EqClass(id)"
		"ON UPDATE RESTRICT ON DELETE RESTRICT);";
   char *err_msg_raw;
   int res = sqlite3_exec(db, sql, NULL, NULL, &err_msg_raw);
   if (res != SQLITE_OK) {
	   auto err_msg = detail::MakeSqliteUnique(err_msg_raw);
	   throw sqlite_exception(db, std::string("Creating results tables: " ) +
			   err_msg.get());
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
