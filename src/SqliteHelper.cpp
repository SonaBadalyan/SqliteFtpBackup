#include "SqliteHelper.h"
#include "Logger.h"
#include <iostream>
#include <random>
#include <sstream>
#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <memory>
#include <stdexcept>
#include <cstdlib> // getenv

namespace {
    struct Person {
        std::string firstName;
        std::string lastName;
        std::string email;
        std::string createdAt;
    };
}

SqliteHelper::SqliteHelper(const std::string& dbPathPrefix) {
    // Generate timestamped filename
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now;
#if defined(_WIN32)
    localtime_s(&tm_now, &t);
#else
    localtime_r(&t, &tm_now);
#endif

    std::ostringstream oss;
    oss << dbPathPrefix << "_"
        << std::put_time(&tm_now, "%Y-%m-%d_%H-%M-%S")
        << ".sqlite";

    dbPath = oss.str();

    Logger::instance().info("Opening SQLite database: " + dbPath);

    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        std::string err = sqlite3_errmsg(db) ? sqlite3_errmsg(db) : "Unknown sqlite open error";
        Logger::instance().error("Can't open SQLite DB: " + err);
        throw std::runtime_error("Can't open SQLite DB: " + err);
    }
}

SqliteHelper::~SqliteHelper() {
    if (db) {
        sqlite3_close(db);
        Logger::instance().info("SQLite database closed: " + dbPath);
    }
}

void SqliteHelper::createTable() {
    Logger::instance().info("Creating table 'people' if not exists...");
    const char* sql = R"(CREATE TABLE IF NOT EXISTS people(
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        first_name TEXT,
        last_name TEXT,
        email TEXT,
        created_at TEXT
    );)";
    char* errMsg = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string e = errMsg ? errMsg : "unknown error";
        sqlite3_free(errMsg);
        Logger::instance().error("Failed to create table: " + e);
        throw std::runtime_error("Failed to create table: " + e);
    }
    Logger::instance().info("Table 'people' ready.");
}

void SqliteHelper::insertRandomRows(int count) {
    Logger::instance().info("Inserting " + std::to_string(count) + " random rows...");

    if (sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to begin transaction");
    }

    sqlite3_stmt* rawStmt = nullptr;
    const char* sql = "INSERT INTO people (first_name,last_name,email,created_at) VALUES (?,?,?,?);";
    if (sqlite3_prepare_v2(db, sql, -1, &rawStmt, nullptr) != SQLITE_OK) {
        Logger::instance().error("Failed to prepare insert statement");
        throw std::runtime_error("Failed to prepare insert statement");
    }

    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> stmt(rawStmt, &sqlite3_finalize);

    // RNG setup
    static thread_local std::mt19937 gen;
    static bool seeded = false;
    if (!seeded) {
        const char* seedEnv = std::getenv("SQLITEHELPER_SEED");
        if (seedEnv) {
            gen.seed(std::stoul(seedEnv));
            Logger::instance().info("Using deterministic RNG seed from SQLITEHELPER_SEED");
        } else {
            gen.seed(std::random_device{}());
        }
        seeded = true;
    }

    std::uniform_int_distribution<> nameDist(0, 9);
    std::uniform_int_distribution<> suffixDist(0, 9999);

    auto makePerson = [&](int i) {
        Person p;
        p.firstName = randomFirstName(nameDist(gen));
        p.lastName = randomLastName(nameDist(gen));
        p.email = p.firstName + "." + p.lastName + std::to_string(suffixDist(gen)) + "@example.com";
        p.createdAt = getCurrentTimestamp();
        return p;
    };

    try {
        for (int i = 0; i < count; ++i) {
            Person p = makePerson(i);

            if (sqlite3_bind_text(stmt.get(), 1, p.firstName.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK ||
                sqlite3_bind_text(stmt.get(), 2, p.lastName.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK ||
                sqlite3_bind_text(stmt.get(), 3, p.email.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK ||
                sqlite3_bind_text(stmt.get(), 4, p.createdAt.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
                throw std::runtime_error("Failed to bind values at row " + std::to_string(i));
            }

            if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
                throw std::runtime_error("Insert failed at row " + std::to_string(i));
            }
            sqlite3_reset(stmt.get());
        }

        if (sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to commit transaction");
        }

        Logger::instance().info("Inserted " + std::to_string(count) + " rows successfully.");
    } catch (...) {
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        Logger::instance().error("Transaction rolled back due to error during insertRandomRows");
        throw;
    }
}

void SqliteHelper::dumpToFile(const std::string& dumpFile) {
    Logger::instance().info("Dumping database to SQL file: " + dumpFile);
    std::ofstream out(dumpFile);
    if (!out.is_open()) {
        throw std::runtime_error("Cannot open dump file: " + dumpFile);
    }

    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, first_name, last_name, email, created_at FROM people;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare select statement for dump");
    }

    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> stmtGuard(stmt, &sqlite3_finalize);

    int rowCount = 0;
    while (sqlite3_step(stmtGuard.get()) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmtGuard.get(), 0);
        const char* first_name = reinterpret_cast<const char*>(sqlite3_column_text(stmtGuard.get(), 1));
        const char* last_name  = reinterpret_cast<const char*>(sqlite3_column_text(stmtGuard.get(), 2));
        const char* email      = reinterpret_cast<const char*>(sqlite3_column_text(stmtGuard.get(), 3));
        const char* created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmtGuard.get(), 4));

        out << "INSERT INTO people (id, first_name, last_name, email, created_at) VALUES ("
            << id << ", '" << first_name << "', '" << last_name << "', '" << email << "', '" << created_at << "');\n";
        ++rowCount;
    }

    Logger::instance().info("Dumped " + std::to_string(rowCount) + " rows to file successfully.");
}

void SqliteHelper::backupToFile(const std::string& dumpFile) {
    Logger::instance().info("Performing binary backup to file: " + dumpFile);

    sqlite3* destDb = nullptr;
    if (sqlite3_open(dumpFile.c_str(), &destDb) != SQLITE_OK) {
        std::string err = sqlite3_errmsg(destDb) ? sqlite3_errmsg(destDb) : "unknown error";
        if (destDb) sqlite3_close(destDb);
        throw std::runtime_error("Failed to open destination DB: " + err);
    }

    std::unique_ptr<sqlite3, decltype(&sqlite3_close)> destGuard(destDb, &sqlite3_close);

    sqlite3_backup* backup = sqlite3_backup_init(destDb, "main", db, "main");
    if (!backup) {
        std::string err = sqlite3_errmsg(destDb) ? sqlite3_errmsg(destDb) : "unknown error";
        throw std::runtime_error("sqlite3_backup_init failed: " + err);
    }

    int rc = SQLITE_OK;
    do {
        rc = sqlite3_backup_step(backup, 1024);
        if (rc == SQLITE_BUSY || rc == SQLITE_LOCKED) {
            sqlite3_sleep(50); // avoid tight loop
        }
    } while (rc == SQLITE_OK || rc == SQLITE_BUSY || rc == SQLITE_LOCKED);

    int rcFinish = sqlite3_backup_finish(backup);
    if (rc != SQLITE_DONE || rcFinish != SQLITE_OK) {
        std::string err = sqlite3_errmsg(destDb) ? sqlite3_errmsg(destDb) : "unknown error";
        throw std::runtime_error("sqlite3_backup failed: " + err);
    }

    Logger::instance().info("Binary backup completed successfully to: " + dumpFile);
}

int SqliteHelper::getRowCount() {
    sqlite3_stmt* rawStmt = nullptr;
    const char* sql = "SELECT COUNT(*) FROM people;";
    if (sqlite3_prepare_v2(db, sql, -1, &rawStmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare count statement");
    }

    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> stmt(rawStmt, &sqlite3_finalize);

    int count = 0;
    if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt.get(), 0);
    }
    Logger::instance().info("Current row count: " + std::to_string(count));
    return count;
}

std::string SqliteHelper::randomFirstName(int idx) const {
    static const std::string names[] = {"Anna","David","Maya","Liam","Sophie","Alex","Nora","Arman","Karen","Sara"};
    if (idx < 0 || idx > 9) idx = 0;
    return names[idx];
}

std::string SqliteHelper::randomLastName(int idx) const {
    static const std::string last[] = {"Petrosyan","Smith","Johnson","Grigoryan","Brown","Martirosian","Lee","Garcia","Ivanov","Khan"};
    if (idx < 0 || idx > 9) idx = 0;
    return last[idx];
}

std::string SqliteHelper::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::tm utc_tm;

#if defined(_WIN32)
    gmtime_s(&utc_tm, &now_c);
#else
    gmtime_r(&now_c, &utc_tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&utc_tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}
