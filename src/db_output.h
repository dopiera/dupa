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
