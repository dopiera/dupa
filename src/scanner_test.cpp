#include <utility>

#include "scanner_int.h"

#include "test_common.h"
#include "gtest/gtest.h"

struct Node;
using NodePtr = std::shared_ptr<Node>;

template <class T>
typename T::value_type Nth(T const &t, size_t n) {
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

struct Node {
  explicit Node(std::string name) : name_(std::move(name)) {}
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

struct File : public Node {
  explicit File(std::string const &name) : Node(name) {}
  bool IsDir() const override { return false; }
};

struct Dir : public Node {
  explicit Dir(std::string const &name) : Node(name) {}
  bool IsDir() const override { return true; }
};

struct TestProcessor : public ScanProcessor<NodePtr> {
  void File(boost::filesystem::path const &path, NodePtr const &parent,
            FileInfo const & /*f_info*/) override {
    parent->entries_.insert(NodePtr(new ::File(path.native())));
  }

  NodePtr RootDir(boost::filesystem::path const &path) override {
    root_.reset(new ::Dir(path.native()));
    return root_;
  }

  NodePtr Dir(boost::filesystem::path const &path,
              NodePtr const &parent) override {
    NodePtr n(new ::Dir(path.native()));
    parent->entries_.insert(n);
    return n;
  }

  NodePtr root_;
};

TEST(DbImport, OneFile) {
  FileInfo const fi(1, 2, 3);
  TestProcessor p;
  ScanDb(
      {
          {"/ala/ma/kota", fi},
      },
      p);
  ASSERT_TRUE(p.root_->IsDir());
  ASSERT_EQ(p.root_->name_, "/ala/ma");
  ASSERT_EQ(p.root_->entries_.size(), 1U);
  ASSERT_EQ((*p.root_->entries_.begin())->name_, "kota");
}

TEST(DbImport, RootPrefix) {
  FileInfo const fi(1, 2, 3);
  TestProcessor p;
  ScanDb(
      {
          {"/ala/ma/kota", fi},
          {"/bob/ma/kota", fi},
      },
      p);
  ASSERT_TRUE(p.root_->IsDir());
  ASSERT_EQ(p.root_->name_, "/");
  ASSERT_EQ(p.root_->Size(), 2U);
  NodePtr const ala = p.root_->Nth(0);
  NodePtr const bob = p.root_->Nth(1);
  ASSERT_EQ(ala->name_, "ala");
  ASSERT_EQ(ala->Size(), 1U);
  ASSERT_EQ(ala->Nth(0)->name_, "ma");
  ASSERT_EQ(ala->Nth(0)->Size(), 1U);
  ASSERT_TRUE(ala->Nth(0)->Nth(0)->IsFile());
  ASSERT_EQ(ala->Nth(0)->Nth(0)->name_, "kota");
  ASSERT_EQ(bob->name_, "bob");
  ASSERT_EQ(bob->Size(), 1U);
  ASSERT_EQ(bob->Nth(0)->name_, "ma");
  ASSERT_EQ(bob->Nth(0)->Size(), 1U);
  ASSERT_TRUE(bob->Nth(0)->Nth(0)->IsFile());
  ASSERT_EQ(bob->Nth(0)->Nth(0)->name_, "kota");
}

TEST(DbImport, EmptyPrefix) {
  FileInfo const fi(1, 2, 3);
  TestProcessor p;
  ScanDb(
      {
          {"ala/ma/kota", fi},
          {"bob/ma/kota", fi},
      },
      p);
  ASSERT_TRUE(p.root_->IsDir());
  ASSERT_EQ(p.root_->name_, "");
  ASSERT_EQ(p.root_->Size(), 2U);
  NodePtr const ala = p.root_->Nth(0);
  NodePtr const bob = p.root_->Nth(1);
  ASSERT_EQ(ala->name_, "ala");
  ASSERT_EQ(ala->Size(), 1U);
  ASSERT_EQ(ala->Nth(0)->name_, "ma");
  ASSERT_EQ(ala->Nth(0)->Size(), 1U);
  ASSERT_TRUE(ala->Nth(0)->Nth(0)->IsFile());
  ASSERT_EQ(ala->Nth(0)->Nth(0)->name_, "kota");
  ASSERT_EQ(bob->name_, "bob");
  ASSERT_EQ(bob->Size(), 1U);
  ASSERT_EQ(bob->Nth(0)->name_, "ma");
  ASSERT_EQ(bob->Nth(0)->Size(), 1U);
  ASSERT_TRUE(bob->Nth(0)->Nth(0)->IsFile());
  ASSERT_EQ(bob->Nth(0)->Nth(0)->name_, "kota");
}

TEST(DbImport, NontrivialPrefix) {
  FileInfo const fi(1, 2, 3);
  TestProcessor p;
  ScanDb(
      {
          {"ala/ma/kota", fi},
          {"ala/ma/psa", fi},
      },
      p);
  ASSERT_TRUE(p.root_->IsDir());
  ASSERT_EQ(p.root_->name_, "ala/ma");
  ASSERT_EQ(p.root_->Size(), 2U);
  NodePtr const ala = p.root_->Nth(0);
  NodePtr const bob = p.root_->Nth(1);
  ASSERT_TRUE(ala->IsFile());
  ASSERT_EQ(ala->name_, "kota");
  ASSERT_TRUE(bob->IsFile());
  ASSERT_EQ(bob->name_, "psa");
}

TEST(DbImport, NontrivialAbsolutePrefix) {
  FileInfo const fi(1, 2, 3);
  TestProcessor p;
  ScanDb(
      {
          {"/ala/ma/kota", fi},
          {"/ala/ma/psa", fi},
      },
      p);
  ASSERT_TRUE(p.root_->IsDir());
  ASSERT_EQ(p.root_->name_, "/ala/ma");
  ASSERT_EQ(p.root_->Size(), 2U);
  NodePtr const ala = p.root_->Nth(0);
  NodePtr const bob = p.root_->Nth(1);
  ASSERT_TRUE(ala->IsFile());
  ASSERT_EQ(ala->name_, "kota");
  ASSERT_TRUE(bob->IsFile());
  ASSERT_EQ(bob->name_, "psa");
}

TEST(DbImport, ComplexTest) {
  FileInfo const fi(1, 2, 3);
  TestProcessor p;
  ScanDb(
      {
          {"/ala/ma/duzego/kota", fi},
          {"/ala/ma/duzego/psa", fi},
          {"/ala/ma/malego/kota", fi},
      },
      p);
  ASSERT_TRUE(p.root_->IsDir());
  ASSERT_EQ(p.root_->name_, "/ala/ma");
  ASSERT_EQ(p.root_->Size(), 2U);
  NodePtr const big = p.root_->Nth(0);
  NodePtr const small = p.root_->Nth(1);
  ASSERT_TRUE(big->IsDir());
  ASSERT_EQ(big->name_, "duzego");
  ASSERT_EQ(big->Size(), 2U);
  ASSERT_EQ(big->Nth(0)->name_, "kota");
  ASSERT_EQ(big->Nth(1)->name_, "psa");
  ASSERT_TRUE(small->IsDir());
  ASSERT_EQ(small->name_, "malego");
  ASSERT_EQ(small->Size(), 1U);
  ASSERT_EQ(small->Nth(0)->name_, "kota");
  ASSERT_EQ(small->Nth(0)->name_, "kota");
}
