#include "db_output.h"

#include <new>

#include "sql_lib.h"

void CreateResultsDatabase(SqliteConnection &db) {
   db.SqliteExec(
	   "DROP TABLE IF EXISTS Node;"
	   "DROP TABLE IF EXISTS EqClass;"
	   "CREATE TABLE EqClass("
	     "id INT PRIMARY KEY     NOT NULL,"
		 "nodes          INT     NOT NULL,"
		 "weight         DOUBLE  NOT NULL,"
	     "interesting    BOOL    NOT NULL);"
	   "CREATE TABLE Node("
         "id INT PRIMARY KEY     NOT NULL,"
         "name            TEXT    NOT NULL,"
         "path            TEXT    NOT NULL,"
         "type            CHAR(5) NOT NULL,"
         "cksum           INTEGER,"  // NULL for directories
         "unique_fraction DOUBLE NOT NULL,"
         "eq_class		  INT     NOT NULL,"
	    "FOREIGN KEY(eq_class) REFERENCES EqClass(id)"
		"ON UPDATE RESTRICT ON DELETE RESTRICT);");
}

void DumpInterestingEqClasses(SqliteConnection &db,
		std::vector<EqClass*> const &eq_classes) {
	char const eq_class_sql[] =
		"UPDATE EqClass SET interesting = 1 WHERE id == ?";
	SqliteConnection::StmtPtr eq_class_stmt(db.PrepareStmt(eq_class_sql));
	db.StartTransaction();
	for (EqClass const *eq_class: eq_classes) {
		SqliteBind(
				*eq_class_stmt,
				reinterpret_cast<uintptr_t>(eq_class)
				);
		int res = sqlite3_step(eq_class_stmt.get());
		if (res != SQLITE_DONE)
			db.Fail("Inserting EqClass");
		res = sqlite3_clear_bindings(eq_class_stmt.get());
		if (res != SQLITE_OK)
		   db.Fail("Clearing EqClass bindings");
		res = sqlite3_reset(eq_class_stmt.get());
		if (res != SQLITE_OK)
		   db.Fail("Clearing EqClass bindings");
	}
	db.EndTransaction();
}

void DumpFuzzyDedupRes(SqliteConnection &db, FuzzyDedupRes const &res) {
	char const eq_class_sql[] =
		"INSERT INTO EqClass(id, nodes, weight, interesting) "
		"VALUES(?, ?, ?, 0)";
	char const node_sql[] =
		"INSERT INTO Node(id, name, path, type, unique_fraction, eq_class) "
			"VALUES(?, ?, ?, ?, ?, ?)";
	SqliteConnection::StmtPtr eq_class_stmt(db.PrepareStmt(eq_class_sql));
	SqliteConnection::StmtPtr node_stmt(db.PrepareStmt(node_sql));
	db.StartTransaction();
	for (EqClass const &eq_class: *res.second) {
		SqliteBind(
				*eq_class_stmt,
				reinterpret_cast<uintptr_t>(&eq_class),
				eq_class.GetNumNodes(),
				eq_class.GetWeight()
				);
		int res = sqlite3_step(eq_class_stmt.get());
		if (res != SQLITE_DONE)
			db.Fail("Inserting EqClass");
		res = sqlite3_clear_bindings(eq_class_stmt.get());
		if (res != SQLITE_OK)
		   db.Fail("Clearing EqClass bindings");
		res = sqlite3_reset(eq_class_stmt.get());
		if (res != SQLITE_OK)
		   db.Fail("Clearing EqClass bindings");
	}
	res.first->Traverse([&] (Node const *n) {
		SqliteBind(
				*node_stmt,
				reinterpret_cast<uintptr_t>(n),
				n->GetName(),
				n->BuildPath().native(),
				std::string((n->GetType() == Node::FILE) ? "FILE" : "DIR"),
				n->unique_fraction,
				reinterpret_cast<uintptr_t>(&n->GetEqClass())
				);
		int res = sqlite3_step(node_stmt.get());
		if (res != SQLITE_DONE)
		   db.Fail("Inserting EqClass");
		res = sqlite3_clear_bindings(node_stmt.get());
		if (res != SQLITE_OK)
		   db.Fail("Clearing EqClass bindings");
		res = sqlite3_reset(node_stmt.get());
		if (res != SQLITE_OK)
		   db.Fail("Clearing EqClass bindings");
	});
	db.EndTransaction();
}
