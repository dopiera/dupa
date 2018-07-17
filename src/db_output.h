#ifndef SRC_DB_OUTPUT_H_
#define SRC_DB_OUTPUT_H_

#include <exception>
#include <memory>
#include <string>

#include "fuzzy_dedup.h"
#include "sql_lib.h"

void CreateResultsDatabase(SqliteConnection &db);
void DumpFuzzyDedupRes(SqliteConnection &db, const FuzzyDedupRes &res);
void DumpInterestingEqClasses(SqliteConnection &db,
                              const std::vector<EqClass *> &eq_classes);

#endif  // SRC_DB_OUTPUT_H_
