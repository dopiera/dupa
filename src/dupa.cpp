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
