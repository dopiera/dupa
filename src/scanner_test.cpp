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

#include <memory>
#include <utility>

#include "scanner_int.h"

#include "gtest/gtest.h"
#include "test_common.h"

class Node;
using NodePtr = std::shared_ptr<Node>;

template <class T>
typename T::value_type Nth(const T &t, size_t n) {
  auto it = t.begin();
  assert(it != t.end());
  for (size_t i = 0; i < n; ++i) {
    ++it;
    assert(it != t.end());
  }
  return *it;
}

struct NodePtrComparator {
  bool operator()(const NodePtr &n1, const NodePtr &n2) const;
};

class Node {
 public:
  explicit Node(std::string name) : name_(std::move(name)) {}
  explicit Node(std::string name, std::set<NodePtr, NodePtrComparator> entries)
      : name_(std::move(name)), entries_(std::move(entries)) {}
  virtual ~Node() = default;

  virtual bool IsFile() const { return !IsDir(); }
  virtual bool IsDir() const = 0;

  size_t Size() const { return entries_.size(); }
  NodePtr Nth(size_t idx) const { return ::Nth(entries_, idx); }

  std::string name_;
  std::set<NodePtr, NodePtrComparator> entries_;
};
bool NodePtrComparator::operator()(const NodePtr &n1, const NodePtr &n2) const {
  return n1->name_ < n2->name_;
}

class File : public Node {
 public:
  explicit File(const std::string &name) : Node(name) {}
  bool IsDir() const override { return false; }
};

class Dir : public Node {
 public:
  explicit Dir(const std::string &name) : Node(name) {}
  explicit Dir(std::string name, std::set<NodePtr, NodePtrComparator> entries)
      : Node(std::move(name), std::move(entries)) {}
  bool IsDir() const override { return true; }
};

NodePtr F(const std::string &name) { return std::make_shared<File>(name); }

NodePtr D(const std::string &name,
          const std::set<NodePtr, NodePtrComparator> &entries) {
  return std::make_shared<Dir>(name, entries);
}

bool CompareTrees(const NodePtr &t1, const NodePtr &t2) {
  if (t1->name_ != t2->name_) {
    return false;
  }
  if (t1->IsDir() != t2->IsDir()) {
    return false;
  }

  auto t1i = t1->entries_.begin(), t2i = t2->entries_.begin();
  for (; t1i != t1->entries_.end() && t2i != t2->entries_.end(); ++t1i, ++t2i) {
    if (!CompareTrees(*t1i, *t2i)) {
      return false;
    }
  }
  return (t1i == t1->entries_.end() && t2i == t2->entries_.end());
}

template <typename S>
void PrintTree(S &stream, const NodePtr &t, std::string indent) {
  if (t->IsDir()) {
    stream << indent << "[" << t->name_ << "]" << std::endl;
    for (const auto &c : t->entries_) {
      PrintTree(stream, c, indent + "  ");
    }
  } else {
    stream << indent << t->name_ << std::endl;
  }
}

bool operator==(const NodePtr &t1, const NodePtr &t2) {
  return CompareTrees(t1, t2);
}

std::ostream &operator<<(std::ostream &stream, const NodePtr &t) {
  PrintTree(stream, t, "");
  return stream;
}

class TestProcessor : public ScanProcessor<NodePtr> {
 public:
  void File(const boost::filesystem::path &path, const NodePtr &parent,
            const FileInfo & /*f_info*/) override {
    parent->entries_.insert(NodePtr(new ::File(path.native())));
  }

  NodePtr RootDir(const boost::filesystem::path &path) override {
    root_.reset(new ::Dir(path.native()));
    return root_;
  }

  NodePtr Dir(const boost::filesystem::path &path,
              const NodePtr &parent) override {
    NodePtr n(new ::Dir(path.native()));
    parent->entries_.insert(n);
    return n;
  }

  NodePtr root_;
};

TEST(DbImport, OneFile) {
  const FileInfo fi(1, 2, 3);
  TestProcessor p;
  ScanDb(
      {
          {"/ala/ma/kota", fi},
      },
      p);
  ASSERT_EQ(p.root_, D("/ala/ma", {F("kota")}));
}

TEST(DbImport, RootPrefix) {
  const FileInfo fi(1, 2, 3);
  TestProcessor p;
  ScanDb(
      {
          {"/ala/ma/kota", fi},
          {"/bob/ma/kota", fi},
      },
      p);
  ASSERT_EQ(p.root_, D("/", {
                                D("ala", {D("ma", {F("kota")})}),
                                D("bob", {D("ma", {F("kota")})}),
                            }));
}

TEST(DbImport, EmptyPrefix) {
  const FileInfo fi(1, 2, 3);
  TestProcessor p;
  ScanDb(
      {
          {"ala/ma/kota", fi},
          {"bob/ma/kota", fi},
      },
      p);
  ASSERT_EQ(p.root_, D("", {
                               D("ala", {D("ma", {F("kota")})}),
                               D("bob", {D("ma", {F("kota")})}),
                           }));
}

TEST(DbImport, NontrivialPrefix) {
  const FileInfo fi(1, 2, 3);
  TestProcessor p;
  ScanDb(
      {
          {"ala/ma/kota", fi},
          {"ala/ma/psa", fi},
      },
      p);
  ASSERT_EQ(p.root_, D("ala/ma", {F("kota"), F("psa")}));
}

TEST(DbImport, NontrivialAbsolutePrefix) {
  const FileInfo fi(1, 2, 3);
  TestProcessor p;
  ScanDb(
      {
          {"/ala/ma/kota", fi},
          {"/ala/ma/psa", fi},
      },
      p);
  ASSERT_EQ(p.root_, D("/ala/ma", {F("kota"), F("psa")}));
}

TEST(DbImport, ComplexTest) {
  const FileInfo fi(1, 2, 3);
  TestProcessor p;
  ScanDb(
      {
          {"/ala/ma/duzego/kota", fi},
          {"/ala/ma/duzego/psa", fi},
          {"/ala/ma/malego/kota", fi},
      },
      p);
  ASSERT_EQ(p.root_, D("/ala/ma", {D("duzego", {F("kota"), F("psa")}),
                                   D("malego", {F("kota")})}));
}

TEST(FileSystem, EmptyDir) {
  TmpDir t;
  TestProcessor p;
  ScanDirectory(t.dir_, p);
  ASSERT_EQ(p.root_, D(t.dir_, {}));
}

TEST(FileSystem, OneEmptyEntry) {
  TmpDir t;
  t.CreateFile("t");
  TestProcessor p;
  HashCache::Initializer hash_cache_init("", "");
  ScanDirectory(t.dir_, p);
  // We're deliberately ignoring 0-long files.
  ASSERT_EQ(p.root_, D(t.dir_, {}));
}

TEST(FileSystem, OneEntryNonEmpty) {
  TmpDir t;
  t.CreateFile("t", "a");
  TestProcessor p;
  HashCache::Initializer hash_cache_init("", "");
  ScanDirectory(t.dir_, p);
  ASSERT_EQ(p.root_, D(t.dir_, {F("t")}));
}

TEST(FileSystem, NestedEmptyDirs) {
  TmpDir t;
  t.CreateSubdir("dir1");
  t.CreateSubdir("dir2");
  t.CreateSubdir("dir2/dir21");
  t.CreateSubdir("dir2/dir22");
  TestProcessor p;
  HashCache::Initializer hash_cache_init("", "");
  ScanDirectory(t.dir_, p);
  ASSERT_EQ(p.root_, D(t.dir_, {D("dir1", {}),
                                D("dir2", {D("dir21", {}), D("dir22", {})})}));
}

TEST(FileSystem, ComplexStructure) {
  TmpDir t;
  t.CreateFile("dir1/file1", "a");
  t.CreateFile("dir1/empty1");
  t.CreateFile("dir1/dir11/file111", "a");
  t.CreateFile("file1", "b");
  t.CreateFile("empty1");
  TestProcessor p;
  HashCache::Initializer hash_cache_init("", "");
  ScanDirectory(t.dir_, p);
  ASSERT_EQ(p.root_,
            D(t.dir_, {D("dir1", {F("file1"), D("dir11", {F("file111")})}),
                       F("file1")}));
}

TEST(FileSystem, NonExistentDir) {
  TmpDir t;
  TestProcessor p;
  HashCache::Initializer hash_cache_init("", "");
  ScanDirectory(t.dir_ + "/nonexistent", p);
  ASSERT_FALSE(p.root_);
}

TEST(FileSystem, DirWithNoReadPerm) {
  TmpDir t;
  TestProcessor p;
  HashCache::Initializer hash_cache_init("", "");
  t.Chmod(".", 0100);
  ScanDirectory(t.dir_, p);
  ASSERT_FALSE(p.root_);
}

TEST(FileSystem, DirWithNoExecPerm) {
  TmpDir t;
  TestProcessor p;
  HashCache::Initializer hash_cache_init("", "");
  t.CreateFile("asd", "a");
  t.Chmod(".", 0400);
  ScanDirectory(t.dir_, p);
  // Successfully lists the directory but fails to analyze its contents.
  ASSERT_EQ(p.root_, D(t.dir_, {}));
}

TEST(FileSystem, FileWithNoReadPerm) {
  TmpDir t;
  TestProcessor p;
  HashCache::Initializer hash_cache_init("", "");
  t.CreateFile("asd", "a");
  t.Chmod("asd", 0000);
  ScanDirectory(t.dir_, p);
  ASSERT_EQ(p.root_, D(t.dir_, {}));
}

TEST(FileSystem, BadFileDoesntAffectOthers) {
  TmpDir t;
  TestProcessor p;
  HashCache::Initializer hash_cache_init("", "");
  t.CreateFile("a", "a");
  t.CreateFile("asd", "a");
  t.Chmod("asd", 0000);
  t.CreateFile("qwe", "a");
  ScanDirectory(t.dir_, p);
  ASSERT_EQ(p.root_, D(t.dir_, {F("a"), F("qwe")}));
}

TEST(FileSystem, DirWithNoReadPermDoesntAffectOthers) {
  TmpDir t;
  TestProcessor p;
  HashCache::Initializer hash_cache_init("", "");
  t.CreateSubdir("asd");
  t.CreateFile("a", "a");
  t.Chmod("asd", 0100);
  t.CreateFile("qwe", "a");
  ScanDirectory(t.dir_, p);
  ASSERT_EQ(p.root_, D(t.dir_, {F("a"), F("qwe")}));
}

TEST(FileSystem, DirWithNoExecPermDoesntAffectOthers) {
  TmpDir t;
  TestProcessor p;
  HashCache::Initializer hash_cache_init("", "");
  t.CreateSubdir("asd");
  t.CreateFile("a", "a");
  t.Chmod("asd", 0400);
  t.CreateFile("qwe", "a");
  ScanDirectory(t.dir_, p);
  ASSERT_EQ(p.root_, D(t.dir_, {F("a"), D("asd", {}), F("qwe")}));
}
