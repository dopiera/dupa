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

#include "fuzzy_dedup.h"

#include <memory>

#include <boost/filesystem/path.hpp>

#include "gtest/gtest.h"

using boost::filesystem::path;

class FuzzyDedupTest : public ::testing::Test {
 public:
  FuzzyDedupTest() : root_node_(new Node(Node::DIR, "/")) {
    nodes_[root_node_->BuildPath().native()] = root_node_.get();
  }

  Node *AddDir(const std::string &native_path) {
    Node *cur_node = root_node_.get();
    path cur_prefix = cur_node->BuildPath();
    for (const path &component : path(native_path)) {
      cur_prefix /= component;
      auto it = nodes_.find(cur_prefix.native());
      if (it != nodes_.end()) {
        cur_node = it->second;
        assert(cur_node->GetType() == Node::DIR);
      } else {
        auto *new_node = new Node(Node::DIR, component.native());
        cur_node->AddChild(new_node);
        assert(new_node->BuildPath() == cur_prefix);
        nodes_[new_node->BuildPath().native()] = new_node;
        cur_node = new_node;
      }
    }
    return cur_node;
  }

  Cksum EqClass2Cksum(const std::string &eq_class) {
    auto res = class_cksums_.insert(std::make_pair(eq_class, unused_cksum_));
    if (res.second) {
      ++unused_cksum_;
    }
    return res.first->second;
  }

  Node *AddFile(const std::string &eq_class, const std::string &native,
                off_t size) {
    path bpath(native);
    Node *parent = AddDir(bpath.parent_path().native());
    Node *new_node = new Node(Node::FILE, bpath.filename().native(), size);
    parent->AddChild(new_node);
    sum2node_.insert(std::make_pair(EqClass2Cksum(eq_class), new_node));
    bool res =
        nodes_.insert(std::make_pair(new_node->BuildPath().native(), new_node))
            .second;
    assert(res);
    (void)(res);
    return new_node;
  }

  void Execute() {
    EqClassesPtr eq_classes =
        detail::ClassifyDuplicateFiles(*root_node_, sum2node_);
    {
      std::unique_ptr<EqClass> empty_dirs_class =
          detail::ClassifyEmptyDirs(*root_node_);
      if (!empty_dirs_class->IsEmpty()) {
        eq_classes->push_back(std::move(empty_dirs_class));
      }
    }
    detail::PropagateEquivalence(*root_node_, eq_classes);
    detail::SortEqClasses(eq_classes);
    detail::CalculateUniqueness(*root_node_);
    res_ = std::make_pair(root_node_, eq_classes);
  }

  Node *FindNode(const std::string &p) {
    auto it = nodes_.find(p);
    assert(it != nodes_.end());
    return it->second;
  }

  void AssertDups(const std::vector<std::string> &paths) {
    for (auto p = paths.begin(); p != paths.end(); p++) {
      auto next = p;
      ++next;
      if (next != paths.end()) {
        Node *n1 = FindNode(*p);
        Node *n2 = FindNode(*next);
        ASSERT_EQ(&n1->GetEqClass(), &n2->GetEqClass());
      }
    }
  }

  void AssertNotDups(const std::vector<std::string> &paths) {
    for (auto p = paths.begin(); p != paths.end(); ++p) {
      auto next = p;
      ++next;
      for (; next != paths.end(); ++next) {
        Node *n1 = FindNode(*p);
        Node *n2 = FindNode(*next);
        ASSERT_NE(&n1->GetEqClass(), &n2->GetEqClass());
      }
    }
  }

 protected:
  void SetUp() override {}
  void TearDown() override {}

  std::unordered_map<std::string, Node *> nodes_;
  std::unordered_map<std::string, Cksum> class_cksums_;
  detail::Sum2Node sum2node_;
  std::shared_ptr<Node> root_node_;
  Cksum unused_cksum_{};
  FuzzyDedupRes res_;
};

TEST_F(FuzzyDedupTest, EmptyDir) {
  Execute();
  ASSERT_EQ(res_.first->BuildPath(), "/");
  ASSERT_EQ(res_.second->size(), 1U);
}

TEST_F(FuzzyDedupTest, JustFiles) {
  AddFile("eq1", "a", 1);
  AddFile("eq1", "b", 1);
  AddFile("eq2", "c", 1);
  Execute();
  AssertDups({"/a", "/b"});
  AssertNotDups({"/a", "/c"});
  AssertNotDups({"/b", "/c"});
}

TEST_F(FuzzyDedupTest, SimpleDirs) {
  AddFile("eq1", "x/a", 1);
  AddFile("eq1", "x/b", 1);
  AddFile("eq2", "x/c", 1);
  AddFile("eq1", "y/a", 1);
  AddFile("eq1", "y/b", 1);
  AddFile("eq2", "y/c", 1);
  AddFile("eq3", "z/a", 1);
  AddFile("eq3", "z/b", 1);
  AddFile("eq3", "z/c", 1);
  Execute();
  AssertDups({"/x", "/y"});
  AssertNotDups({"/x", "/z"});
  AssertNotDups({"/y", "/z"});
}

TEST_F(FuzzyDedupTest, ScatteredDir) {
  AddFile("1", "x/a", 1);
  AddFile("2", "y/a", 1);
  AddFile("3", "z/a", 1);
  AddFile("4", "u/a", 1);

  AddFile("1", "v/a", 1);
  AddFile("2", "v/b", 1);
  AddFile("3", "v/c", 1);
  AddFile("4", "v/d", 1);

  Execute();
  ASSERT_DOUBLE_EQ(FindNode("/v")->unique_fraction_, 0);
}

TEST_F(FuzzyDedupTest, UniqueDir) {
  AddFile("1", "x/a", 1);
  AddFile("2", "y/a", 1);
  AddFile("3", "z/a", 1);
  AddFile("4", "u/a", 1);

  AddFile("5", "v/a", 1);
  AddFile("6", "v/b", 1);
  AddFile("7", "v/c", 1);
  AddFile("8", "v/d", 1);

  Execute();
  ASSERT_DOUBLE_EQ(FindNode("/v")->unique_fraction_, 1);
}

TEST_F(FuzzyDedupTest, MostlyScatteredDir) {
  AddFile("1", "x/a", 1);
  AddFile("2", "y/a", 1);
  AddFile("3", "z/a", 1);
  AddFile("4", "u/a", 1);

  AddFile("1", "v/a", 1);
  AddFile("2", "v/b", 1);
  AddFile("3", "v/c", 1);
  AddFile("5", "v/d", 1);

  Execute();
  ASSERT_DOUBLE_EQ(FindNode("/v")->unique_fraction_, .25);
}
