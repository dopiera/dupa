#include "sql_lib_impl.h"

#include <cstdlib>

#include <boost/filesystem/operations.hpp>

#include "exceptions.h"
#include "log.h"
#include "gtest/gtest.h"

class TmpDir {
public:
  TmpDir() {
    char tmp_dir[] = "/tmp/dupa.XXXXXX";
    if (!mkdtemp(tmp_dir)) {
      throw fs_exception(errno, "Creating a temp directory");
    }
    this->dir = tmp_dir;
  }

  ~TmpDir() {
    try {
      boost::filesystem::remove_all(this->dir);
    } catch (const boost::filesystem::filesystem_error &e) {
      LOG(ERROR, std::string("Failed to remove temp test directory "
                             "because (") +
                     e.what() + "), leaving garbage behind (" + this->dir +
                     ")");
    }
  }

  std::string dir;
};

class SqliteTest : public ::testing::Test {
public:
  SqliteTest()
      : db(tmp.dir + "/db.sqlite3"), data{std::make_tuple(1, 0.1, "one"),
                                          std::make_tuple(2, 0.2, "two"),
                                          std::make_tuple(3, 0.3, "three"),
                                          std::make_tuple(4, 0.4, "four"),
                                          std::make_tuple(5, 0.5, "five")} {}
  void CreateTable() {
    this->db.SqliteExec("CREATE TABLE Tbl("
                        "id INT PRIMARY KEY     NOT NULL,"
                        "dbl            DOUBLE  NOT NULL,"
                        "txt            TEXT    NOT NULL"
                        ");");
  }
  void InsertValues() {
    auto out_stream = this->db.BatchInsert<int, double, std::string>(
        "INSERT INTO Tbl VALUES(?, ?, ?);");
    std::copy(this->data.begin(), this->data.end(), out_stream->begin());
  }
  std::vector<std::tuple<int, float, std::string>> QueryAllValues() {
    std::vector<std::tuple<int, float, std::string>> res;
    auto in_stream = this->db.Query<int, float, std::string>(
        "SELECT * FROM Tbl ORDER BY id;");
    std::copy(in_stream.begin(), in_stream.end(), std::back_inserter(res));
    return res;
  }

protected:
  TmpDir tmp;
  SqliteConnection db;
  std::vector<std::tuple<int, float, char const *>> data;
};

TEST_F(SqliteTest, TableCreate) { this->CreateTable(); }

TEST_F(SqliteTest, DoubleTableCreate) {
  this->CreateTable();
  EXPECT_THROW(this->CreateTable(), sqlite_exception);
}

TEST_F(SqliteTest, Inserting) {
  this->CreateTable();
  this->InsertValues();
}

TEST_F(SqliteTest, InputIterator) {
  this->CreateTable();
  this->InsertValues();
  auto res =
      this->db.Query<int, float, std::string>("SELECT * FROM Tbl ORDER BY id;");
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
  this->CreateTable();
  auto res =
      this->db.Query<int, float, std::string>("SELECT * FROM Tbl ORDER BY id;");
  ASSERT_EQ(res.begin(), res.end());
}

TEST_F(SqliteTest, Querying) {
  this->CreateTable();
  this->InsertValues();
  auto res = this->QueryAllValues();

  ASSERT_EQ(this->data.size(), res.size());
  auto diff = std::mismatch(this->data.begin(), this->data.end(), res.begin());

  ASSERT_EQ(diff.first, this->data.end());
  ASSERT_EQ(diff.second, res.end());
}

TEST_F(SqliteTest, InsertFail) {
  this->CreateTable();
  this->InsertValues();

  // duplicate key, should fail
  EXPECT_THROW(this->db.SqliteExec("INSERT INTO Tbl VALUES(4, 4.0, \"four\");"),
               sqlite_exception);

  auto res = this->QueryAllValues();

  ASSERT_EQ(this->data.size(), res.size());
  auto diff = std::mismatch(this->data.begin(), this->data.end(), res.begin());

  ASSERT_EQ(diff.first, this->data.end());
  ASSERT_EQ(diff.second, res.end());
}

TEST_F(SqliteTest, SuccessfulTransaction) {
  this->CreateTable();
  {
    SqliteTransaction trans(this->db);
    this->InsertValues();
    trans.Commit();
  }

  auto res = this->QueryAllValues();

  ASSERT_EQ(this->data.size(), res.size());
  auto diff = std::mismatch(this->data.begin(), this->data.end(), res.begin());

  ASSERT_EQ(diff.first, this->data.end());
  ASSERT_EQ(diff.second, res.end());
}

TEST_F(SqliteTest, AbortedTransaction) {
  this->CreateTable();
  {
    SqliteTransaction trans(this->db);
    this->InsertValues();
    trans.Rollback();
  }

  auto res = this->QueryAllValues();

  ASSERT_TRUE(res.empty());
}

TEST_F(SqliteTest, AbortedByExceptionTransaction) {
  this->CreateTable();
  {
    SqliteTransaction trans(this->db);
    this->InsertValues();
    // SqliteTransaction is automatically destroyed like in an exception.
  }

  auto res = this->QueryAllValues();

  ASSERT_TRUE(res.empty());
}
