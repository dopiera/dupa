#include "scanner_int.h"

#include "gtest/gtest.h"
#include "test_common.h"

struct Node;
typedef std::shared_ptr<Node> NodePtr;

template <class T>
typename T::value_type nth(T const & t, size_t n) {
	typename T::const_iterator it = t.begin();
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
	Node(std::string const & name) : name(name) {}
	virtual ~Node() {}

	virtual bool IsFile() const { return !IsDir(); }
	virtual bool IsDir() const = 0;

	size_t Size() const { return entries.size(); }
	NodePtr nth(size_t idx) const { return ::nth(entries, idx); }

	std::string name;
	std::set<NodePtr, NodePtrComparator> entries;
};

bool NodePtrComparator::operator()(const NodePtr &n1, const NodePtr &n2) const {
	return n1->name < n2->name;
}

struct File : public Node {
	File(std::string const & name) : Node(name) {}
	virtual bool IsDir() const { return false; }
};

struct Dir : public Node {
	Dir(std::string const & name) : Node(name) {}
	virtual bool IsDir() const { return true; }
};

struct TestProcessor : public ScanProcessor<NodePtr> {
	virtual void File(
			boost::filesystem::path const &path,
			NodePtr const &parent,
			file_info const &f_info) {
		parent->entries.insert(NodePtr(new ::File(path.native())));
	}

	virtual NodePtr RootDir(boost::filesystem::path const &path) {
		root.reset(new ::Dir(path.native()));
		return root;
	}

	virtual NodePtr Dir(
			boost::filesystem::path const &path,
			NodePtr const &parent
			) {
		NodePtr n(new ::Dir(path.native()));
		parent->entries.insert(n);
		return n;
	}

	NodePtr root;
};

TEST(DbImport, OneFile) {
	file_info const fi(1, 2, 3);
	TestProcessor p;
	ScanDb({
			{"/ala/ma/kota", fi},
			},
			p);
	ASSERT_TRUE(p.root->IsDir());
	ASSERT_EQ(p.root->name, "/ala/ma");
	ASSERT_EQ(p.root->entries.size(), 1U);
	ASSERT_EQ((*p.root->entries.begin())->name, "kota");
}

TEST(DbImport, RootPrefix) {
	file_info const fi(1, 2, 3);
	TestProcessor p;
	ScanDb({
			{"/ala/ma/kota", fi},
			{"/bob/ma/kota", fi},
			},
			p);
	ASSERT_TRUE(p.root->IsDir());
	ASSERT_EQ(p.root->name, "/");
	ASSERT_EQ(p.root->Size(), 2U);
	NodePtr const ala = p.root->nth(0);
	NodePtr const bob = p.root->nth(1);
	ASSERT_EQ(ala->name, "ala");
	ASSERT_EQ(ala->Size(), 1U);
	ASSERT_EQ(ala->nth(0)->name, "ma");
	ASSERT_EQ(ala->nth(0)->Size(), 1U);
	ASSERT_TRUE(ala->nth(0)->nth(0)->IsFile());
	ASSERT_EQ(ala->nth(0)->nth(0)->name, "kota");
	ASSERT_EQ(bob->name, "bob");
	ASSERT_EQ(bob->Size(), 1U);
	ASSERT_EQ(bob->nth(0)->name, "ma");
	ASSERT_EQ(bob->nth(0)->Size(), 1U);
	ASSERT_TRUE(bob->nth(0)->nth(0)->IsFile());
	ASSERT_EQ(bob->nth(0)->nth(0)->name, "kota");
}

TEST(DbImport, EmptyPrefix) {
	file_info const fi(1, 2, 3);
	TestProcessor p;
	ScanDb({
			{"ala/ma/kota", fi},
			{"bob/ma/kota", fi},
			},
			p);
	ASSERT_TRUE(p.root->IsDir());
	ASSERT_EQ(p.root->name, "");
	ASSERT_EQ(p.root->Size(), 2U);
	NodePtr const ala = p.root->nth(0);
	NodePtr const bob = p.root->nth(1);
	ASSERT_EQ(ala->name, "ala");
	ASSERT_EQ(ala->Size(), 1U);
	ASSERT_EQ(ala->nth(0)->name, "ma");
	ASSERT_EQ(ala->nth(0)->Size(), 1U);
	ASSERT_TRUE(ala->nth(0)->nth(0)->IsFile());
	ASSERT_EQ(ala->nth(0)->nth(0)->name, "kota");
	ASSERT_EQ(bob->name, "bob");
	ASSERT_EQ(bob->Size(), 1U);
	ASSERT_EQ(bob->nth(0)->name, "ma");
	ASSERT_EQ(bob->nth(0)->Size(), 1U);
	ASSERT_TRUE(bob->nth(0)->nth(0)->IsFile());
	ASSERT_EQ(bob->nth(0)->nth(0)->name, "kota");
}

TEST(DbImport, NontrivialPrefix) {
	file_info const fi(1, 2, 3);
	TestProcessor p;
	ScanDb({
			{"ala/ma/kota", fi},
			{"ala/ma/psa", fi},
			},
			p);
	ASSERT_TRUE(p.root->IsDir());
	ASSERT_EQ(p.root->name, "ala/ma");
	ASSERT_EQ(p.root->Size(), 2U);
	NodePtr const ala = p.root->nth(0);
	NodePtr const bob = p.root->nth(1);
	ASSERT_TRUE(ala->IsFile());
	ASSERT_EQ(ala->name, "kota");
	ASSERT_TRUE(bob->IsFile());
	ASSERT_EQ(bob->name, "psa");
}

TEST(DbImport, NontrivialAbsolutePrefix) {
	file_info const fi(1, 2, 3);
	TestProcessor p;
	ScanDb({
			{"/ala/ma/kota", fi},
			{"/ala/ma/psa", fi},
			},
			p);
	ASSERT_TRUE(p.root->IsDir());
	ASSERT_EQ(p.root->name, "/ala/ma");
	ASSERT_EQ(p.root->Size(), 2U);
	NodePtr const ala = p.root->nth(0);
	NodePtr const bob = p.root->nth(1);
	ASSERT_TRUE(ala->IsFile());
	ASSERT_EQ(ala->name, "kota");
	ASSERT_TRUE(bob->IsFile());
	ASSERT_EQ(bob->name, "psa");
}

TEST(DbImport, ComplexTest) {
	file_info const fi(1, 2, 3);
	TestProcessor p;
	ScanDb({
			{"/ala/ma/duzego/kota", fi},
			{"/ala/ma/duzego/psa", fi},
			{"/ala/ma/malego/kota", fi},
			},
			p);
	ASSERT_TRUE(p.root->IsDir());
	ASSERT_EQ(p.root->name, "/ala/ma");
	ASSERT_EQ(p.root->Size(), 2U);
	NodePtr const big = p.root->nth(0);
	NodePtr const small = p.root->nth(1);
	ASSERT_TRUE(big->IsDir());
	ASSERT_EQ(big->name, "duzego");
	ASSERT_EQ(big->Size(), 2U);
	ASSERT_EQ(big->nth(0)->name, "kota");
	ASSERT_EQ(big->nth(1)->name, "psa");
	ASSERT_TRUE(small->IsDir());
	ASSERT_EQ(small->name, "malego");
	ASSERT_EQ(small->Size(), 1U);
	ASSERT_EQ(small->nth(0)->name, "kota");
	ASSERT_EQ(small->nth(0)->name, "kota");
}
