#include "gtest/gtest.h"
#include "SqliteHelper.h"
#include "Logger.h"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>

// ----------------------------
// Fixture for SqliteHelper tests
// ----------------------------
class SqliteHelperTest : public ::testing::Test {
public:
    const std::string& getDbPath() const { return dbPath; }
protected:
    std::string dbPath;
    SqliteHelper* dbHelper;

    // Generate timestamped database filename
    std::string generateDbPath() {
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        std::tm tm_now;
#if defined(_WIN32)
        localtime_s(&tm_now, &t);
#else
        localtime_r(&t, &tm_now);
#endif
        std::ostringstream oss;
        oss << "test_db_" << std::put_time(&tm_now, "%Y-%m-%d_%H-%M-%S") << ".sqlite";
        return oss.str();
    }

    void SetUp() override {
        dbPath = generateDbPath();
        dbHelper = new SqliteHelper(dbPath);
    }

    void TearDown() override {
        delete dbHelper;
        if (std::filesystem::exists(dbPath)) {
            std::filesystem::remove(dbPath); // cleanup after test
        }
    }
};

// ----------------------------
// Tests
// ----------------------------
TEST_F(SqliteHelperTest, CreateTableWorks) {
    EXPECT_NO_THROW(dbHelper->createTable());
}

TEST_F(SqliteHelperTest, InsertRandomRowsWorks) {
    dbHelper->createTable();
    EXPECT_NO_THROW(dbHelper->insertRandomRows(10));
    int rowCount = dbHelper->getRowCount();
    EXPECT_EQ(rowCount, 10);
}
