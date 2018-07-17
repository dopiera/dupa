#include "db_output.h"

#include <new>

#include "db_lib_impl.h"

void CreateResultsDatabase(DBConnection &db) {
  db.Exec(
      "DROP TABLE IF EXISTS Node;"
      "DROP TABLE IF EXISTS EqClass;"
      "CREATE TABLE EqClass("
      "id INT PRIMARY  KEY     NOT NULL,"
      "nodes           INT     NOT NULL,"
      "weight          DOUBLE  NOT NULL,"
      "interesting     BOOL    NOT NULL);"
      "CREATE TABLE Node("
      "id INT PRIMARY  KEY     NOT NULL,"
      "name            TEXT    NOT NULL,"
      "path            TEXT    NOT NULL,"
      "type            CHAR(5) NOT NULL,"
      "cksum           INTEGER,"  // NULL for directories
      "unique_fraction DOUBLE  NOT NULL,"
      "eq_class        INT     NOT NULL,"
      "FOREIGN KEY(eq_class) REFERENCES EqClass(id)"
      "ON UPDATE RESTRICT ON DELETE RESTRICT);");
}

void DumpInterestingEqClasses(DBConnection &db,
                              const std::vector<EqClass *> &eq_classes) {
  DBTransaction trans(db);
  auto out =
      db.Prepare<uintptr_t>("UPDATE EqClass SET interesting = 1 WHERE id == ?");
  std::transform(
      eq_classes.begin(), eq_classes.end(), out->begin(),
      [](EqClass *eq_class_ptr) {
        return std::make_tuple(reinterpret_cast<uintptr_t>(eq_class_ptr));
      });
  trans.Commit();
}

void DumpFuzzyDedupRes(DBConnection &db, const FuzzyDedupRes &res) {
  DBTransaction trans(db);

  auto class_out = db.Prepare<uintptr_t, size_t, double>(
      "INSERT INTO EqClass(id, nodes, weight, interesting) "
      "VALUES(?, ?, ?, 0)");

  std::transform(res.second->begin(), res.second->end(), class_out->begin(),
                 [](const std::unique_ptr<EqClass> &eq_class) {
                   return std::make_tuple(
                       reinterpret_cast<uintptr_t>(eq_class.get()),
                       eq_class->GetNumNodes(), eq_class->GetWeight());
                 });

  auto node_out = db.Prepare<uintptr_t, std::string, std::string, std::string,
                             double, uintptr_t>(
      "INSERT INTO Node("
      "id, name, path, type, unique_fraction, eq_class) "
      "VALUES(?, ?, ?, ?, ?, ?)");
  res.first->Traverse([&](const Node *n) {
    node_out->Write(
        reinterpret_cast<uintptr_t>(n), n->GetName(), n->BuildPath().native(),
        (n->GetType() == Node::FILE) ? "FILE" : "DIR", n->unique_fraction_,
        reinterpret_cast<uintptr_t>(&n->GetEqClass()));
  });
  trans.Commit();
}
