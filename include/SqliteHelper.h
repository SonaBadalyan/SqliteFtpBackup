#pragma once
#include <sqlite3.h>
#include <string>
#include <stdexcept>

class SqliteHelper {
public:
    /**
     * Constructor opens (or creates) the SQLite database at dbPathPrefix
     * The final database path will include a timestamp suffix.
     * @param dbPathPrefix - prefix for the SQLite database file
     * @throws std::runtime_error if the database cannot be opened
     */
    explicit SqliteHelper(const std::string& dbPathPrefix);

    /** Destructor closes the SQLite database */
    ~SqliteHelper();

    /**
     * Create a sample table if it does not exist
     * @throws std::runtime_error on failure
     */
    void createTable();

    /**
     * Insert random rows into the database
     * @param count - number of rows to insert
     * @throws std::runtime_error on failure
     */
    void insertRandomRows(int count);

    /**
     * Get the number of rows in the main table
     * @return row count
     * @throws std::runtime_error on failure
     */
    int getRowCount();

    /**
     * Dump the entire database to a separate file (SQL statements)
     * @param dumpFile - path to the dump file
     * @throws std::runtime_error on failure
     */
    void dumpToFile(const std::string& dumpFile);

    /**
     * Perform a binary backup of the entire database to a file
     * using the sqlite3_backup API (more efficient than SQL dump).
     * @param dumpFile - path to the backup file
     * @throws std::runtime_error on failure
     */
    void backupToFile(const std::string& dumpFile);

    /** Return path to the database */
    std::string getDbPath() const { return dbPath; }

private:
    sqlite3* db = nullptr;
    std::string dbPath;

    /** Generate random first name for sample data */
    std::string randomFirstName(int idx) const;

    /** Generate random last name for sample data */
    std::string randomLastName(int idx) const;

    /** Get current timestamp as ISO 8601 string */
    std::string getCurrentTimestamp() const;
};
