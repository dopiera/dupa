#ifndef DB_OUTPUT_H_12662
#define DB_OUTPUT_H_12662

#include <exception>
#include <memory>
#include <string>

#include "sql_lib.h"
#include "fuzzy_dedup.h"

void CreateResultsDatabase(SqliteConnection &db);
void DumpFuzzyDedupRes(SqliteConnection &db, FuzzyDedupRes const &res);
void DumpInterestingEqClasses(SqliteConnection &db,
		std::vector<EqClass*> const &eq_classes);

#endif /* DB_OUTPUT_H_12662 */
