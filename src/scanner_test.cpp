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

NodePtr F(const std::string name) { return std::make_shared<File>(name); }

NodePtr D(const std::string &name,
          const std::set<NodePtr, NodePtrComparator> &entries) {
  return std::make_shared<Dir>(name, entries);
}

bool CompareTrees(NodePtr t1, NodePtr t2) {
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
  if (t1i == t1->entries_.end() && t2i == t2->entries_.end()) {
    return true;
  }
  return false;
}

template <typename S>
void PrintTree(S &stream, const NodePtr &t, std::string indent) {
  if (t->IsDir()) {
    stream << indent << "[" << t->name_ << "]" << std::endl;
    for (const auto c : t->entries_) {
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
