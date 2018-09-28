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

#include <fstream>
#include <string>
#include <thread>

#include <utility>

#include "conf.h"
#include "db_lib.h"
#include "db_output.h"
#include "dir_compare.h"
#include "file_tree.h"
#include "fuzzy_dedup.h"
#include "hash_cache.h"
#include "log.h"

static_assert(sizeof(off_t) == 8);

static void PrintCompilationProfileWarning() {
  LogLevel ll = stderr_loglevel;
  stderr_loglevel = DEBUG;
  DLOG("This is a debug build, performance might suck.");
  stderr_loglevel = ll;
}

int main(int argc, char **argv) {
  PrintCompilationProfileWarning();
  ParseArgv(argc, argv);

  try {
    HashCache::Initializer hash_cache_init(Conf().read_cache_from_,
                                           Conf().dump_cache_to_);

    if (Conf().cache_only_) {
      for (const auto &dir : Conf().dirs_) {
        WarmupCache(dir);
      }
      return 0;
    }
    // Open database first to catch configuration issues soon.
    std::unique_ptr<DBConnection> db(
        Conf().sql_out_.empty() ? nullptr : new DBConnection(Conf().sql_out_));
    if (Conf().dirs_.size() == 1) {
      FuzzyDedupRes res = FuzzyDedup(Conf().dirs_[0]);
      if (!res.first) {
        // no nodes at all
        std::cout << "No files in specified location" << std::endl;
      } else {
        auto eq_classes = GetInteresingEqClasses(res);
        PrintEqClassses(eq_classes);
        PrintScatteredDirectories(*res.first);
        if (!!db) {
          LOG(INFO, "Dumping results to " << Conf().sql_out_);
          CreateResultsDatabase(*db);
          DumpFuzzyDedupRes(*db, res);
          DumpInterestingEqClasses(*db, eq_classes);
        }
      }
    } else if (Conf().dirs_.size() == 2) {
      PrintingOutputStream stdout;
      std::unique_ptr<DirCompDBStream> db_stream(db ? new DirCompDBStream(*db)
                                                    : nullptr);
      std::vector<std::reference_wrapper<CompareOutputStream>> streams_v;
      streams_v.push_back(std::ref(static_cast<CompareOutputStream &>(stdout)));
      if (db_stream) {
        streams_v.push_back(
            std::ref(static_cast<CompareOutputStream &>(*db_stream)));
      }
      CompareOutputStreams streams(std::move(streams_v));

      DirCompare(Conf().dirs_[0], Conf().dirs_[1], streams);
      if (db_stream) {
        db_stream->Commit();
      }
      return 0;
    } else {
      // Should have been checked already.
      assert(false);
    }
  } catch (const std::ios_base::failure &ex) {
    std::cerr << "Failure: " << ex.what() << std::endl;
    throw;
  }

  return 0;
}
