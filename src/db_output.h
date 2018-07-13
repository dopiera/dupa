#ifndef DB_OUTPUT_H_12662
#define DB_OUTPUT_H_12662

#include <exception>
#include <memory>
#include <string>

#include "fuzzy_dedup.h"
#include "sql_lib.h"

void CreateResultsDatabase(SqliteConnection &db);
void DumpFuzzyDedupRes(SqliteConnection &db, FuzzyDedupRes const &res);
void DumpInterestingEqClasses(SqliteConnection &db,
                              std::vector<EqClass *> const &eq_classes);

#endif /* DB_OUTPUT_H_12662 */
