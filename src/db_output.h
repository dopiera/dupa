#ifndef SRC_DB_OUTPUT_H_
#define SRC_DB_OUTPUT_H_

#include <exception>
#include <memory>
#include <string>

#include "db_lib.h"
#include "fuzzy_dedup.h"

void CreateResultsDatabase(DBConnection &db);
void DumpFuzzyDedupRes(DBConnection &db, const FuzzyDedupRes &res);
void DumpInterestingEqClasses(DBConnection &db,
                              const std::vector<EqClass *> &eq_classes);

#endif  // SRC_DB_OUTPUT_H_
