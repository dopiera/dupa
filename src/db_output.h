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

#ifndef SRC_DB_OUTPUT_H_
#define SRC_DB_OUTPUT_H_

#include <exception>
#include <memory>
#include <string>

#include "db_lib.h"
#include "dir_compare.h"
#include "fuzzy_dedup.h"

void CreateResultsDatabase(DBConnection &db);
void DumpFuzzyDedupRes(DBConnection &db, const FuzzyDedupRes &res);
void DumpInterestingEqClasses(DBConnection &db,
                              const std::vector<EqClass *> &eq_classes);

// For printing directory comparison result.
class DirCompDBStream : public CompareOutputStream {
 public:
  explicit DirCompDBStream(DBConnection &conn);
  void OverwrittenBy(const std::string &f,
                     const std::vector<std::string> &candidates) override;
  void CopiedFrom(const std::string &f,
                  const std::vector<std::string> &candidates) override;
  void RenameTo(const std::string &f,
                const std::vector<std::string> &candidates) override;
  void ContentChanged(const std::string &f) override;
  void Removed(const std::string &f) override;
  void NewFile(const std::string &f) override;
  void Commit();

 private:
  DBConnection &conn_;
  DBTransaction trans_;
  std::unique_ptr<DBOutStream<std::string>> removed_;
  std::unique_ptr<DBOutStream<std::string>> new_file_;
  std::unique_ptr<DBOutStream<std::string>> content_changed_;
  std::unique_ptr<DBOutStream<std::string, std::string>> overwritten_by_;
  std::unique_ptr<DBOutStream<std::string, std::string>> copied_from_;
  std::unique_ptr<DBOutStream<std::string, std::string>> rename_to_;
};

#endif  // SRC_DB_OUTPUT_H_
