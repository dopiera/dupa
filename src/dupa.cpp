#include <fstream>
#include <string>
#include <thread>

#include <boost/filesystem/convenience.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <utility>

#include "conf.h"
#include "db_lib.h"
#include "db_output.h"
#include "file_tree.h"
#include "fuzzy_dedup.h"
#include "hash_cache.h"
#include "log.h"
#include "scanner_int.h"
#include "synch_thread_pool.h"

namespace fs = boost::filesystem;
namespace mi = boost::multi_index;

static_assert(sizeof(off_t) == 8);

struct PathHash {
  PathHash(std::string path, Cksum hash)
      : path_(std::move(path)), hash_(hash) {}

  std::string path_;
  Cksum hash_;
};

struct ByPath {};
struct ByHash {};
using PathHashes = mi::multi_index_container<
    PathHash,
    mi::indexed_by<
        mi::ordered_unique<mi::tag<ByPath>,
                           mi::member<PathHash, std::string, &PathHash::path_>>,
        mi::ordered_non_unique<mi::tag<ByHash>,
                               mi::member<PathHash, Cksum, &PathHash::hash_>>>>;
using PathHashesByPath = PathHashes::index<ByPath>::type;
using PathHashesByHash = PathHashes::index<ByHash>::type;

class PathHashesFiller : public ScanProcessor<fs::path> {
 public:
  explicit PathHashesFiller(PathHashes &hashes) : hashes_(hashes) {}

  void File(const fs::path &path, const fs::path &parent,
            const FileInfo &f_info) override {
    const fs::path relative = parent / path.filename();
    hashes_.insert(PathHash(relative.native(), f_info.sum_));
  }
  fs::path RootDir(const fs::path & /*path*/) override { return fs::path(); }
  fs::path Dir(const fs::path &path, const fs::path &parent) override {
    return parent / path.filename();
  }

 private:
  PathHashes &hashes_;
};

void FillPathHashes(const std::string &start_dir, PathHashes &hashes) {
  PathHashesFiller processor(hashes);
  ScanDirectoryOrDb(start_dir, processor);
}

using Paths = std::vector<std::string>;

template <class S>
S &operator<<(S &stream, const Paths &p) {
  stream << "[";
  for (auto it = p.begin(); it != p.end(); ++it) {
    if (it != p.begin()) {
      stream << " ";
    }
    stream << *it;
  }
  stream << "]";
  return stream;
}

Paths GetPathsForHash(PathHashesByHash &ps, Cksum hash) {
  Paths res;
  for (auto r = ps.equal_range(hash); r.first != r.second; ++r.first) {
    res.push_back(r.first->path_);
  }
  return res;
}

void DirCompare(const fs::path &dir1, const fs::path &dir2) {
  PathHashes hashes1, hashes2;

  std::thread h1filler(
      [&dir1, &hashes1]() { FillPathHashes(dir1.native(), hashes1); });
  std::thread h2filler(
      [&dir2, &hashes2]() { FillPathHashes(dir2.native(), hashes2); });
  h1filler.join();
  h2filler.join();

  PathHashesByPath &hashes1p(hashes1.get<ByPath>());
  PathHashesByPath &hashes2p(hashes2.get<ByPath>());
  PathHashesByHash &hashes1h(hashes1.get<ByHash>());
  PathHashesByHash &hashes2h(hashes2.get<ByHash>());

  for (const auto &path_and_hash : hashes1p) {
    const std::string &p1 = path_and_hash.path_;
    Cksum h1 = path_and_hash.hash_;
    const PathHashesByPath::const_iterator same_path = hashes2p.find(p1);
    if (same_path != hashes2p.end()) {
      // this path exists in second dir
      const Cksum h2 = same_path->hash_;
      if (h1 == h2) {
        // std::cout << "NOT_CHANGED: " << p1 << std::endl;
      } else {
        Paths ps = GetPathsForHash(hashes1h, h2);
        if (!ps.empty()) {
          // rename from somewhere:
          std::cout << "OVERWRITTEN_BY: " << p1 << " CANDIDATES: " << ps
                    << std::endl;
        } else {
          std::cout << "CONTENT_CHANGED: " << p1 << std::endl;
        }
      }
    } else {
      Paths ps = GetPathsForHash(hashes2h, h1);
      if (!ps.empty()) {
        if (!Conf().skip_renames_) {
          std::cout << "RENAME: " << p1 << " -> " << ps << std::endl;
        }
      } else {
        std::cout << "REMOVED: " << p1 << std::endl;
      }
    }
  }
  for (const auto &path_and_hash : hashes2p) {
    const std::string &p2 = path_and_hash.path_;
    Cksum h2 = path_and_hash.hash_;
    if (hashes1p.find(p2) != hashes1p.end()) {
      // path exists in both, so it has already been handled by the first
      // loop
      continue;
    }
    Paths ps = GetPathsForHash(hashes1h, h2);
    if (!ps.empty()) {
      Paths ps2;
      for (const auto &copy_candidate : ps) {
        if (hashes2p.find(copy_candidate) != hashes2p.end()) {
          ps2.push_back(copy_candidate);
        }
        // otherwise it's probably renamed from that file, so it's
        // already mentioned
      }
      if (!ps2.empty()) {
        std::cout << "COPIED_FROM: " << p2 << " CANDIDATES: " << ps2
                  << std::endl;
      }
    } else {
      std::cout << "NEW_FILE: " << p2 << std::endl;
    }
  }
}

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
        PathHashes hashes;
        FillPathHashes(dir, hashes);
      }
      return 0;
    }
    if (Conf().dirs_.size() == 1) {
      // Open database first to catch configuration issues soon.
      std::unique_ptr<DBConnection> db(Conf().sql_out_.empty()
                                           ? nullptr
                                           : new DBConnection(Conf().sql_out_));
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
      DirCompare(Conf().dirs_[0], Conf().dirs_[1]);
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
