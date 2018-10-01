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

#include "dir_compare.h"

#include <cstdlib>

#include "db_lib_impl.h"
#include "db_output.h"
#include "exceptions.h"
#include "gtest/gtest.h"
#include "hash_cache.h"
#include "log.h"
#include "test_common.h"

class DirCompareTest : public ::testing::Test {
 protected:
  TmpDir d1_;
  TmpDir d2_;
  TmpDir db_dir_;
};

template <class T>
std::set<T> Vector2Set(const std::vector<T> &v) {
  return std::set<T>(v.begin(), v.end());
}

class CompareOutputStreamMock : public CompareOutputStream {
 public:
  void OverwrittenBy(const std::string &f,
                     const std::vector<std::string> &candidates) override {
    [[maybe_unused]] const bool inserted =
        overwritten_by_.insert(std::make_pair(f, Vector2Set(candidates)))
            .second;
    assert(inserted);
  }
  void CopiedFrom(const std::string &f,
                  const std::vector<std::string> &candidates) override {
    [[maybe_unused]] const bool inserted =
        copied_from_.insert(std::make_pair(f, Vector2Set(candidates))).second;
    assert(inserted);
  }
  void RenameTo(const std::string &f,
                const std::vector<std::string> &candidates) override {
    [[maybe_unused]] const bool inserted =
        rename_to_.insert(std::make_pair(f, Vector2Set(candidates))).second;
    assert(inserted);
  }
  void ContentChanged(const std::string &f) override {
    [[maybe_unused]] const bool inserted = content_changed_.insert(f).second;
    assert(inserted);
  }
  void Removed(const std::string &f) override {
    [[maybe_unused]] const bool inserted = removed_.insert(f).second;
    assert(inserted);
  }
  void NewFile(const std::string &f) override {
    [[maybe_unused]] const bool inserted = new_file_.insert(f).second;
    assert(inserted);
  }

  std::set<std::pair<std::string, std::set<std::string>>> overwritten_by_;
  std::set<std::pair<std::string, std::set<std::string>>> copied_from_;
  std::set<std::pair<std::string, std::set<std::string>>> rename_to_;
  std::set<std::string> content_changed_;
  std::set<std::string> removed_;
  std::set<std::string> new_file_;
};

TEST_F(DirCompareTest, EmptyDirs) {
  HashCache::Initializer hash_cache_init("", "");
  CompareOutputStreamMock res;
  DirCompare(d1_.dir_, d2_.dir_, res);
  ASSERT_TRUE(res.overwritten_by_.empty());
  ASSERT_TRUE(res.copied_from_.empty());
  ASSERT_TRUE(res.rename_to_.empty());
  ASSERT_TRUE(res.content_changed_.empty());
  ASSERT_TRUE(res.removed_.empty());
  ASSERT_TRUE(res.new_file_.empty());
}

TEST_F(DirCompareTest, IdenticalDirs) {
  HashCache::Initializer hash_cache_init("", "");
  CompareOutputStreamMock res;

  d1_.CreateFile("d", "567");
  d2_.CreateFile("d", "567");
  d1_.CreateFile("d2", "567");
  d2_.CreateFile("d2", "567");
  d1_.CreateFile(" ", "987");
  d2_.CreateFile(" ", "987");

  DirCompare(d1_.dir_, d2_.dir_, res);
  ASSERT_TRUE(res.overwritten_by_.empty());
  ASSERT_TRUE(res.copied_from_.empty());
  ASSERT_TRUE(res.rename_to_.empty());
  ASSERT_TRUE(res.content_changed_.empty());
  ASSERT_TRUE(res.removed_.empty());
  ASSERT_TRUE(res.new_file_.empty());
}

TEST_F(DirCompareTest, MissingAndNew) {
  HashCache::Initializer hash_cache_init("", "");

  d1_.CreateFile("d", "567");
  d2_.CreateFile("d", "567");
  d1_.CreateFile("d2", "567");
  d2_.CreateFile("d2", "567");
  d1_.CreateFile(" ", "987");
  d2_.CreateFile(" ", "987");

  d1_.CreateFile("a", "123");
  d2_.CreateFile("b", "321");

  CompareOutputStreamMock res;
  DirCompare(d1_.dir_, d2_.dir_, res);
  ASSERT_TRUE(res.overwritten_by_.empty());
  ASSERT_TRUE(res.copied_from_.empty());
  ASSERT_TRUE(res.rename_to_.empty());
  ASSERT_TRUE(res.content_changed_.empty());
  ASSERT_EQ(res.removed_, std::set<std::string>{"a"});
  ASSERT_EQ(res.new_file_, std::set<std::string>{"b"});
}

TEST_F(DirCompareTest, ContentChanged) {
  HashCache::Initializer hash_cache_init("", "");

  d1_.CreateFile("d", "567");
  d2_.CreateFile("d", "567");
  d1_.CreateFile("d2", "567");
  d2_.CreateFile("d2", "567");
  d1_.CreateFile(" ", "987");
  d2_.CreateFile(" ", "987");

  d1_.CreateFile("a", "123");
  d2_.CreateFile("a", "321");

  CompareOutputStreamMock res;
  DirCompare(d1_.dir_, d2_.dir_, res);
  ASSERT_TRUE(res.overwritten_by_.empty());
  ASSERT_TRUE(res.copied_from_.empty());
  ASSERT_TRUE(res.rename_to_.empty());
  ASSERT_EQ(res.content_changed_, std::set<std::string>{"a"});
  ASSERT_TRUE(res.removed_.empty());
  ASSERT_TRUE(res.new_file_.empty());
}

TEST_F(DirCompareTest, Rename) {
  HashCache::Initializer hash_cache_init("", "");

  d1_.CreateFile("d", "567");
  d2_.CreateFile("d", "567");
  d1_.CreateFile("d2", "567");
  d2_.CreateFile("d2", "567");
  d1_.CreateFile(" ", "987");
  d2_.CreateFile(" ", "987");

  d1_.CreateFile("a", "123");
  d2_.CreateFile("b", "123");
  d2_.CreateFile("c", "123");

  CompareOutputStreamMock res;
  DirCompare(d1_.dir_, d2_.dir_, res);
  ASSERT_TRUE(res.overwritten_by_.empty());
  ASSERT_TRUE(res.copied_from_.empty());

  std::set<std::pair<std::string, std::set<std::string>>> exp{
      std::make_pair("a", std::set<std::string>{"b", "c"})};
  ASSERT_EQ(res.rename_to_, exp);
  ASSERT_TRUE(res.content_changed_.empty());
  ASSERT_TRUE(res.removed_.empty());
  ASSERT_TRUE(res.new_file_.empty());
}

TEST_F(DirCompareTest, CopiedFrom) {
  HashCache::Initializer hash_cache_init("", "");

  d1_.CreateFile("d", "567");
  d2_.CreateFile("d", "567");
  d1_.CreateFile("d2", "567");
  d2_.CreateFile("d2", "567");
  d1_.CreateFile(" ", "987");
  d2_.CreateFile(" ", "987");

  d1_.CreateFile("a", "123");
  d1_.CreateFile("b", "123");
  d2_.CreateFile("a", "123");
  d2_.CreateFile("b", "123");
  d2_.CreateFile("c", "123");

  CompareOutputStreamMock res;
  DirCompare(d1_.dir_, d2_.dir_, res);
  ASSERT_TRUE(res.overwritten_by_.empty());
  std::set<std::pair<std::string, std::set<std::string>>> exp{
      std::make_pair("c", std::set<std::string>{"a", "b"})};
  ASSERT_EQ(res.copied_from_, exp);
  ASSERT_TRUE(res.rename_to_.empty());
  ASSERT_TRUE(res.content_changed_.empty());
  ASSERT_TRUE(res.removed_.empty());
  ASSERT_TRUE(res.new_file_.empty());
}

TEST_F(DirCompareTest, OverwrittenBy) {
  HashCache::Initializer hash_cache_init("", "");

  d1_.CreateFile("d", "567");
  d2_.CreateFile("d", "567");
  d1_.CreateFile("d2", "567");
  d2_.CreateFile("d2", "567");
  d1_.CreateFile(" ", "987");
  d2_.CreateFile(" ", "987");

  d1_.CreateFile("a", "123");
  d1_.CreateFile("b", "123");
  d1_.CreateFile("c", "000");

  d2_.CreateFile("a", "123");
  d2_.CreateFile("b", "123");
  d2_.CreateFile("c", "123");

  CompareOutputStreamMock res;
  DirCompare(d1_.dir_, d2_.dir_, res);
  std::set<std::pair<std::string, std::set<std::string>>> exp{
      std::make_pair("c", std::set<std::string>{"a", "b"})};
  ASSERT_EQ(res.overwritten_by_, exp);
  ASSERT_TRUE(res.copied_from_.empty());
  ASSERT_TRUE(res.rename_to_.empty());
  ASSERT_TRUE(res.content_changed_.empty());
  ASSERT_TRUE(res.removed_.empty());
  ASSERT_TRUE(res.new_file_.empty());
}

std::set<std::string> ReadStringList(DBConnection &conn,
                                     const std::string &table) {
  std::set<std::string> res_set;

  auto res = conn.Query<std::string>("SELECT * FROM " + table + ";");
  std::transform(
      res.begin(), res.end(), std::inserter(res_set, res_set.end()),
      [](const std::tuple<std::string> &s) { return std::get<0>(s); });
  return res_set;
}

std::set<std::pair<std::string, std::set<std::string>>> ReadStringToStringList(
    DBConnection &conn, const std::string &table) {
  std::map<std::string, std::set<std::string>> m;

  for (const auto &[path, candidate] :
       conn.Query<std::string, std::string>("SELECT * FROM " + table + ";")) {
    [[maybe_unused]] const bool inserted = m[path].insert(candidate).second;
    assert(inserted);
  }
  std::set<std::pair<std::string, std::set<std::string>>> r(m.begin(), m.end());
  return r;
}

TEST_F(DirCompareTest, DBOutput) {
  HashCache::Initializer hash_cache_init("", "");

  // No output for generated these
  d1_.CreateFile("d", "567");
  d2_.CreateFile("d", "567");
  d1_.CreateFile("d2", "567");
  d2_.CreateFile("d2", "567");
  d1_.CreateFile(" ", "987");
  d2_.CreateFile(" ", "987");

  d1_.CreateFile("copied_from_src1", "123");
  d1_.CreateFile("copied_from_src2", "123");
  d2_.CreateFile("copied_from_src1", "123");
  d2_.CreateFile("copied_from_src2", "123");
  d2_.CreateFile("copied_from_target", "123");
  // COPIED_FROM copied_from_target [copied_from_src{1,2}]

  d1_.CreateFile("overwritten_by_src1", "124");
  d1_.CreateFile("overwritten_by_src2", "124");
  d1_.CreateFile("overwritten_by_target", "-124");
  d2_.CreateFile("overwritten_by_src1", "124");
  d2_.CreateFile("overwritten_by_src2", "124");
  d2_.CreateFile("overwritten_by_target", "124");
  // OVERWRITTEN_BY overwritten_by_target [overwritten_by_src{1,2}]

  d1_.CreateFile("rename_to_src", "125");
  d2_.CreateFile("rename_to_target2", "125");
  d2_.CreateFile("rename_to_target1", "125");
  // RENAME_TO rename_to_src [rename_to_target{1,2}]

  d2_.CreateFile("new_file", "126");
  // NEW_FILE new_file
  d1_.CreateFile("removed_file", "127");
  // REMOVED "removed_file"
  d1_.CreateFile("content_changed", "-128");
  d2_.CreateFile("content_changed", "128");
  // CONTENT_CHANGED "content_changed"

  const std::string db_path(db_dir_.dir_ + "/db.sqlite3");
  DBConnection db(db_path);
  DirCompDBStream db_stream(db);
  DirCompare(d1_.dir_, d2_.dir_, db_stream);
  db_stream.Commit();

  using Strings = std::set<std::string>;
  using StringToStringSet =
      std::set<std::pair<std::string, std::set<std::string>>>;
  ASSERT_EQ(ReadStringList(db, "NewFile"), Strings{"new_file"});
  ASSERT_EQ(ReadStringList(db, "Removed"), Strings{"removed_file"});
  ASSERT_EQ(ReadStringList(db, "ContentChanged"), Strings{"content_changed"});
  ASSERT_EQ(ReadStringToStringList(db, "OverwrittenBy"),
            StringToStringSet{std::make_pair(
                "overwritten_by_target",
                Strings{"overwritten_by_src1", "overwritten_by_src2"})});
  ASSERT_EQ(ReadStringToStringList(db, "CopiedFrom"),
            StringToStringSet{std::make_pair(
                "copied_from_target",
                Strings{"copied_from_src1", "copied_from_src2"})});
  ASSERT_EQ(
      ReadStringToStringList(db, "RenameTo"),
      StringToStringSet{std::make_pair(
          "rename_to_src", Strings{"rename_to_target1", "rename_to_target2"})});
}
