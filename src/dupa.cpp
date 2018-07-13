#include <fstream>
#include <string>
#include <thread>

#include <boost/filesystem/convenience.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>

#include "conf.h"
#include "db_output.h"
#include "file_tree.h"
#include "fuzzy_dedup.h"
#include "hash_cache.h"
#include "log.h"
#include "scanner_int.h"
#include "sql_lib.h"
#include "synch_thread_pool.h"

using namespace std;
using namespace boost;
using namespace boost::filesystem;
using namespace boost::multi_index;

BOOST_STATIC_ASSERT(sizeof(off_t) == 8);

struct path_hash {
  path_hash(string const &path, cksum hash) : path(path), hash(hash) {}

  string path;
  cksum hash;
};

struct by_path {};
struct by_hash {};
typedef multi_index_container<
    path_hash,
    indexed_by<ordered_unique<tag<by_path>,
                              member<path_hash, std::string, &path_hash::path>>,
               ordered_non_unique<tag<by_hash>,
                                  member<path_hash, cksum, &path_hash::hash>>>>
    path_hashes;
typedef path_hashes::index<by_path>::type path_hashes_by_path;
typedef path_hashes::index<by_hash>::type path_hashes_by_hash;

struct PathHashesFiller : ScanProcessor<boost::filesystem::path> {
  typedef boost::filesystem::path Path;

  PathHashesFiller(path_hashes &hashes) : hashes_(hashes) {}

  virtual void File(boost::filesystem::path const &path, Path const &parent,
                    file_info const &f_info) {
    Path const relative = parent / path.filename();
    hashes_.insert(path_hash(relative.native(), f_info.sum));
  }
  virtual Path RootDir(boost::filesystem::path const &path) { return Path(); }
  virtual Path Dir(boost::filesystem::path const &path, Path const &parent) {
    return parent / path.filename();
  }

  path_hashes &hashes_;
};

void fill_path_hashes(std::string const &start_dir, path_hashes &hashes) {
  PathHashesFiller processor(hashes);
  ScanDirectoryOrDb(start_dir, processor);
}

typedef vector<string> paths;

template <class S> S &operator<<(S &stream, paths const &p) {
  stream << "[";
  for (paths::const_iterator it = p.begin(); it != p.end(); ++it) {
    if (it != p.begin())
      stream << " ";
    stream << *it;
  }
  stream << "]";
  return stream;
}

paths get_paths_for_hash(path_hashes_by_hash &ps, cksum hash) {
  paths res;
  for (auto r = ps.equal_range(hash); r.first != r.second; ++r.first) {
    res.push_back(r.first->path);
  }
  return res;
}

void dir_compare(path const &dir1, path const &dir2) {
  path_hashes hashes1, hashes2;

  std::thread h1filler(
      std::bind(fill_path_hashes, std::ref(dir1.native()), std::ref(hashes1)));
  std::thread h2filler(
      std::bind(fill_path_hashes, std::ref(dir2.native()), std::ref(hashes2)));
  h1filler.join();
  h2filler.join();

  path_hashes_by_path &hashes1p(hashes1.get<by_path>());
  path_hashes_by_path &hashes2p(hashes2.get<by_path>());
  path_hashes_by_hash &hashes1h(hashes1.get<by_hash>());
  path_hashes_by_hash &hashes2h(hashes2.get<by_hash>());

  for (auto const &path_and_hash : hashes1p) {
    string const &p1 = path_and_hash.path;
    cksum h1 = path_and_hash.hash;
    path_hashes_by_path::const_iterator const same_path = hashes2p.find(p1);
    if (same_path != hashes2p.end()) {
      // this path exists in second dir
      cksum const h2 = same_path->hash;
      if (h1 == h2) {
        // cout << "NOT_CHANGED: " << p1 << endl;
      } else {
        paths ps = get_paths_for_hash(hashes1h, h2);
        if (not ps.empty()) {
          // rename from somewhere:
          cout << "OVERWRITTEN_BY: " << p1 << " CANDIDATES: " << ps << endl;
        } else {
          cout << "CONTENT_CHANGED: " << p1 << endl;
        }
      }
    } else {
      paths ps = get_paths_for_hash(hashes2h, h1);
      if (not ps.empty()) {
        if (!Conf().skip_renames) {
          cout << "RENAME: " << p1 << " -> " << ps << endl;
        }
      } else {
        cout << "REMOVED: " << p1 << endl;
      }
    }
  }
  for (auto const &path_and_hash : hashes2p) {
    string const &p2 = path_and_hash.path;
    cksum h2 = path_and_hash.hash;
    if (hashes1p.find(p2) != hashes1p.end()) {
      // path exists in both, so it has already been handled by the first
      // loop
      continue;
    } else {
      paths ps = get_paths_for_hash(hashes1h, h2);
      if (not ps.empty()) {
        paths ps2;
        for (auto const &copy_candidate : ps) {
          if (hashes2p.find(copy_candidate) != hashes2p.end()) {
            ps2.push_back(copy_candidate);
          }
          // otherwise it's probably renamed from that file, so it's
          // already mentioned
        }
        if (not ps2.empty()) {
          cout << "COPIED_FROM: " << p2 << " CANDIDATES: " << ps2 << endl;
        }
      } else {
        cout << "NEW_FILE: " << p2 << endl;
      }
    }
  }
}

static void print_compilation_profile_warning() {
  LogLevel ll = stderr_loglevel;
  stderr_loglevel = DEBUG;
  DLOG("This is a debug build, performance might suck.");
  stderr_loglevel = ll;
}

int main(int argc, char **argv) {
  print_compilation_profile_warning();
  ParseArgv(argc, argv);

  try {
    hash_cache::initializer hash_cache_init(Conf().read_cache_from,
                                            Conf().dump_cache_to);

    if (Conf().cache_only) {
      for (auto const &dir : Conf().dirs) {
        path_hashes hashes;
        fill_path_hashes(dir, hashes);
      }
      return 0;
    }
    if (Conf().dirs.size() == 1) {
      // Open database first to catch configuration issues soon.
      std::unique_ptr<SqliteConnection> db(
          Conf().sql_out.empty() ? nullptr
                                 : new SqliteConnection(Conf().sql_out));
      FuzzyDedupRes res = fuzzy_dedup(Conf().dirs[0]);
      if (!res.first) {
        // no nodes at all
        std::cout << "No files in specified location" << std::endl;
      } else {
        auto eq_classes = GetInteresingEqClasses(res);
        PrintEqClassses(eq_classes);
        PrintScatteredDirectories(*res.first);
        if (!!db) {
          LOG(INFO, "Dumping results to " << Conf().sql_out);
          CreateResultsDatabase(*db);
          DumpFuzzyDedupRes(*db, res);
          DumpInterestingEqClasses(*db, eq_classes);
        }
      }
    } else if (Conf().dirs.size() == 2) {
      dir_compare(Conf().dirs[0], Conf().dirs[1]);
      return 0;
    } else {
      // Should have been checked already.
      assert(false);
    }
  } catch (ios_base::failure const &ex) {
    cerr << "Failure: " << ex.what() << endl;
    throw;
  }

  return 0;
}
