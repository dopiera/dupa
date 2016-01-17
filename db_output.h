#ifndef DB_OUTPUT_H_12662
#define DB_OUTPUT_H_12662

#include <exception>
#include <memory>
#include <string>

#include "sqlite3.h"
#include "fuzzy_dedup.h"

void CreateResultsDatabase(sqlite3 *db);
void DumpFuzzyDedupRes(sqlite3 *db, FuzzyDedupRes const &res);
void DumpInterestingEqClasses(sqlite3 *db,
		std::vector<EqClass*> const &eq_classes);

#endif /* DB_OUTPUT_H_12662 */
