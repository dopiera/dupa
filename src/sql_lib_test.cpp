#include "sql_lib_impl.h"

#include <cstdlib>

#include <boost/filesystem/operations.hpp>

#include "exceptions.h"
#include "gtest/gtest.h"
#include "log.h"

class TmpDir {
 public:
  TmpDir() {
    char tmp_dir[] = "/tmp/dupa.XXXXXX";
    if (!mkdtemp(tmp_dir)) {
      throw FsException(errno, "Creating a temp directory");
    }
    dir_ = tmp_dir;
  }

  ~TmpDir() {
    try {
      boost::filesystem::remove_all(dir_);
    } catch (const boost::filesystem::filesystem_error &e) {
      LOG(ERROR, std::string("Failed to remove temp test directory "
                             "because (") +
                     e.what() + "), leaving garbage behind (" + dir_ + ")");
    }
  }

  std::string dir_;
};

class SqliteTest : public ::testing::Test {
 public:
  SqliteTest()
      : db_(tmp_.dir_ + "/db.sqlite3"),
        data_{std::make_tuple(1, 0.1, "one"), std::make_tuple(2, 0.2, "two"),
              std::make_tuple(3, 0.3, "three"), std::make_tuple(4, 0.4, "four"),
              std::make_tuple(5, 0.5, "five")} {}
  void CreateTable() {
    db_.SqliteExec(
        "CREATE TABLE Tbl("
        "id INT PRIMARY KEY     NOT NULL,"
        "dbl            DOUBLE  NOT NULL,"
        "txt            TEXT    NOT NULL"
        ");");
  }
  void InsertValues() {
    auto out_stream = db_.BatchInsert<int, double, std::string>(
        "INSERT INTO Tbl VALUES(?, ?, ?);");
    std::copy(data_.begin(), data_.end(), out_stream->begin());
  }
  std::vector<std::tuple<int, float, std::string>> QueryAllValues() {
    std::vector<std::tuple<int, float, std::string>> res;
    auto in_stream =
        db_.Query<int, float, std::string>("SELECT * FROM Tbl ORDER BY id;");
    std::copy(in_stream.begin(), in_stream.end(), std::back_inserter(res));
    return res;
  }

 protected:
  TmpDir tmp_;
  SqliteConnection db_;
  std::vector<std::tuple<int, float, char const *>> data_;
};

TEST_F(SqliteTest, TableCreate) { CreateTable(); }

TEST_F(SqliteTest, DoubleTableCreate) {
  CreateTable();
  EXPECT_THROW(CreateTable(), SqliteException);
}

TEST_F(SqliteTest, Inserting) {
  CreateTable();
  InsertValues();
}

TEST_F(SqliteTest, InputIterator) {
  CreateTable();
  InsertValues();
  auto res =
      db_.Query<int, float, std::string>("SELECT * FROM Tbl ORDER BY id;");
  ASSERT_EQ(res.end(), res.end());

  // Stupid gtest macros fail when given template instantiation, because they
  // contain commas.
  using InputIt = SqliteInputIt<int, float, std::string>;
  ASSERT_EQ(InputIt(), res.end());
  auto it = res.begin();
  ASSERT_EQ(std::get<0>(*it), 1);
  ASSERT_NE(it, res.end());
  ASSERT_NE(InputIt(), it);
  ASSERT_EQ(it, it);
  auto it2 = it;
  ASSERT_NE(it2, res.end());
  ASSERT_EQ(it2, it2);
  ASSERT_EQ(it, it2);
  // So far the iterator has consumed 1 element from the stream.
  ASSERT_EQ(std::get<0>(*it), std::get<0>(*it2));
  it2 = it++;
  ASSERT_EQ(std::get<0>(*it), 2);
  ASSERT_NE(std::get<0>(*it), std::get<0>(*it2));
  // Now this is weird, but all valid iterators are equal
  ASSERT_EQ(it, it2);
  it2 = ++it;
  ASSERT_NE(it, res.end());
  ASSERT_EQ(std::get<0>(*it), 3);
  ASSERT_EQ(std::get<0>(*it), std::get<0>(*it2));
  it2 = ++it;
  ASSERT_NE(it, res.end());
  ASSERT_EQ(std::get<0>(*it), 4);
  ASSERT_EQ(std::get<0>(*it), std::get<0>(*it2));
  it++;
  ASSERT_NE(it, res.end());
  ASSERT_EQ(std::get<0>(*it), 5);
  ASSERT_NE(std::get<0>(*it), std::get<0>(*it2));
  it++;
  ASSERT_EQ(it, res.end());
  ++it2;
  ASSERT_EQ(it2, res.end());
}

TEST_F(SqliteTest, EmptyInputIterator) {
  CreateTable();
  auto res =
      db_.Query<int, float, std::string>("SELECT * FROM Tbl ORDER BY id;");
  ASSERT_EQ(res.begin(), res.end());
}

TEST_F(SqliteTest, Querying) {
  CreateTable();
  InsertValues();
  auto res = QueryAllValues();

  ASSERT_EQ(data_.size(), res.size());
  auto diff = std::mismatch(data_.begin(), data_.end(), res.begin());

  ASSERT_EQ(diff.first, data_.end());
  ASSERT_EQ(diff.second, res.end());
}

TEST_F(SqliteTest, InsertFail) {
  CreateTable();
  InsertValues();

  // duplicate key, should fail
  EXPECT_THROW(db_.SqliteExec("INSERT INTO Tbl VALUES(4, 4.0, \"four\");"),
               SqliteException);

  auto res = QueryAllValues();

  ASSERT_EQ(data_.size(), res.size());
  auto diff = std::mismatch(data_.begin(), data_.end(), res.begin());

  ASSERT_EQ(diff.first, data_.end());
  ASSERT_EQ(diff.second, res.end());
}

TEST_F(SqliteTest, SuccessfulTransaction) {
  CreateTable();
  {
    SqliteTransaction trans(db_);
    InsertValues();
    trans.Commit();
  }

  auto res = QueryAllValues();

  ASSERT_EQ(data_.size(), res.size());
  auto diff = std::mismatch(data_.begin(), data_.end(), res.begin());

  ASSERT_EQ(diff.first, data_.end());
  ASSERT_EQ(diff.second, res.end());
}

TEST_F(SqliteTest, AbortedTransaction) {
  CreateTable();
  {
    SqliteTransaction trans(db_);
    InsertValues();
    trans.Rollback();
  }

  auto res = QueryAllValues();

  ASSERT_TRUE(res.empty());
}

TEST_F(SqliteTest, AbortedByExceptionTransaction) {
  CreateTable();
  {
    SqliteTransaction trans(db_);
    InsertValues();
    // SqliteTransaction is automatically destroyed like in an exception.
  }

  auto res = QueryAllValues();

  ASSERT_TRUE(res.empty());
}
