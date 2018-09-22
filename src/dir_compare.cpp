#include "dir_compare.h"

#include <boost/filesystem/convenience.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>

#include "conf.h"
#include "hash_cache.h"
#include "scanner_int.h"
#include "synch_thread_pool.h"

namespace mi = boost::multi_index;
namespace fs = boost::filesystem;

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

void PrintingOutputStream::OverwrittenBy(
    const std::string &f, const std::vector<std::string> &candidates) const {
  std::cout << "OVERWRITTEN_BY: " << f << " CANDIDATES: " << candidates
            << std::endl;
}

void PrintingOutputStream::CopiedFrom(
    const std::string &f, const std::vector<std::string> &candidates) const {
  std::cout << "COPIED_FROM: " << f << " CANDIDATES: " << candidates
            << std::endl;
}

void PrintingOutputStream::Rename(const std::string &f,
                                  const std::vector<std::string> &to) const {
  std::cout << "RENAME: " << f << " -> " << to << std::endl;
}

void PrintingOutputStream::ContentChanged(const std::string &f) const {
  std::cout << "CONTENT_CHANGED: " << f << std::endl;
}

void PrintingOutputStream::Removed(const std::string &f) const {
  std::cout << "REMOVED: " << f << std::endl;
}

void PrintingOutputStream::NewFile(const std::string &f) const {
  std::cout << "NEW_FILE: " << f << std::endl;
}

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

PathHashes FillPathHashes(const std::string &start_dir) {
  PathHashes res;
  PathHashesFiller processor(res);
  ScanDirectoryOrDb(start_dir, processor);
  return res;
}

void WarmupCache(const std::string &path) { FillPathHashes(path); }

Paths GetPathsForHash(PathHashesByHash &ps, Cksum hash) {
  Paths res;
  for (auto r = ps.equal_range(hash); r.first != r.second; ++r.first) {
    res.push_back(r.first->path_);
  }
  return res;
}

void DirCompare(const std::string &dir1, const std::string &dir2,
                const CompareOutputStream &stream) {
  PathHashes hashes1, hashes2;

  std::thread h1filler([&dir1, &hashes1]() { hashes1 = FillPathHashes(dir1); });
  std::thread h2filler([&dir2, &hashes2]() { hashes2 = FillPathHashes(dir2); });
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
          stream.OverwrittenBy(p1, ps);
        } else {
          stream.ContentChanged(p1);
        }
      }
    } else {
      Paths ps = GetPathsForHash(hashes2h, h1);
      if (!ps.empty()) {
        if (!Conf().skip_renames_) {
          stream.Rename(p1, ps);
        }
      } else {
        stream.Removed(p1);
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
        stream.CopiedFrom(p2, ps2);
      }
    } else {
      stream.NewFile(p2);
    }
  }
}
