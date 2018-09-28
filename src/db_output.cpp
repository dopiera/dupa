/*
 * (C) Copyright 2018 Marek Dopiera
 *
 * This file is part of dupa.
 *
 * dupa is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * dupa is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with dupa. If not, see http://www.gnu.org/licenses/.
 */

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

DirCompDBStream::DirCompDBStream(DBConnection &conn)
    : conn_(conn), trans_(conn) {
  conn.Exec(
      "DROP TABLE IF EXISTS Removed;"
      "DROP TABLE IF EXISTS NewFile;"
      "DROP TABLE IF EXISTS ContentChanged;"
      "DROP TABLE IF EXISTS OverwrittenBy;"
      "DROP TABLE IF EXISTS CopiedFrom;"
      "DROP TABLE IF EXISTS RenameTo;"
      "CREATE TABLE Removed("
      "path            TEXT    NOT NULL UNIQUE);"
      "CREATE TABLE NewFile("
      "path            TEXT    NOT NULL UNIQUE);"
      "CREATE TABLE ContentChanged("
      "path            TEXT    NOT NULL UNIQUE);"
      "CREATE TABLE OverwrittenBy("
      "path            TEXT    NOT NULL,"
      "candidate_by    TEXT    NOT NULL);"
      "CREATE TABLE CopiedFrom("
      "path            TEXT    NOT NULL,"
      "candidate_from  TEXT    NOT NULL);"
      "CREATE TABLE RenameTo("
      "path            TEXT    NOT NULL,"
      "candidate_to    TEXT    NOT NULL);");
  removed_ = std::move(
      conn.Prepare<std::string>("INSERT INTO Removed(path) VALUES(?)"));
  new_file_ = std::move(
      conn.Prepare<std::string>("INSERT INTO NewFile(path) VALUES(?)"));
  content_changed_ = std::move(
      conn.Prepare<std::string>("INSERT INTO ContentChanged(path) VALUES(?)"));
  overwritten_by_ = std::move(conn.Prepare<std::string, std::string>(
      "INSERT INTO OverwrittenBy(path, candidate_by) VALUES(?, ?)"));
  copied_from_ = std::move(conn.Prepare<std::string, std::string>(
      "INSERT INTO CopiedFrom(path, candidate_from) VALUES(?, ?)"));
  rename_to_ = std::move(conn.Prepare<std::string, std::string>(
      "INSERT INTO RenameTo(path, candidate_to) VALUES(?, ?)"));
}

void DirCompDBStream::OverwrittenBy(
    const std::string &f, const std::vector<std::string> &candidates) {
  for (const auto &c : candidates) {
    overwritten_by_->Write(f, c);
  }
}

void DirCompDBStream::CopiedFrom(const std::string &f,
                                 const std::vector<std::string> &candidates) {
  for (const auto &c : candidates) {
    copied_from_->Write(f, c);
  }
}

void DirCompDBStream::RenameTo(const std::string &f,
                               const std::vector<std::string> &candidates) {
  for (const auto &c : candidates) {
    rename_to_->Write(f, c);
  }
}

void DirCompDBStream::ContentChanged(const std::string &f) {
  content_changed_->Write(f);
}

void DirCompDBStream::Removed(const std::string &f) { removed_->Write(f); }

void DirCompDBStream::NewFile(const std::string &f) { new_file_->Write(f); }

void DirCompDBStream::Commit() { trans_.Commit(); }
