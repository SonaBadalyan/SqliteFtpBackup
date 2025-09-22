#include "Logger.h"
#include <gtest/gtest.h>
#include <fstream>
#include <regex>
#include <filesystem>

// Helper to read the last line of a file
std::string readLastLine(const std::string& path) {
    std::ifstream file(path);
    std::string line, last;
    while (std::getline(file, line)) {
        last = line;
    }
    return last;
}

TEST(LoggerTest, LevelFiltering) {
    Logger& logger = Logger::instance();
    logger.setLevel(Logger::Level::WARNING);

    // This debug/info should be ignored
    logger.debug("Debug message");
    logger.info("Info message");
    
    // These should be logged
    logger.warn("Warning message");
    logger.error("Error message");

    // Find the latest log file
    std::string latestLog;
    for (auto& p : std::filesystem::directory_iterator("logs")) {
        if (!latestLog.empty() && p.path().filename() <= latestLog) continue;
        latestLog = p.path().filename().string();
    }
    std::string logPath = "logs/" + latestLog;

    std::string lastLine = readLastLine(logPath);
    EXPECT_TRUE(lastLine.find("Error message") != std::string::npos);
    EXPECT_TRUE(lastLine.find("ERROR") != std::string::npos);
}

TEST(LoggerTest, TimestampFormat) {
    Logger& logger = Logger::instance();
    logger.setLevel(Logger::Level::DEBUG);
    logger.debug("Test timestamp");

    std::string latestLog;
    for (auto& p : std::filesystem::directory_iterator("logs")) {
        if (!latestLog.empty() && p.path().filename() <= latestLog) continue;
        latestLog = p.path().filename().string();
    }
    std::string logPath = "logs/" + latestLog;
    std::string lastLine = readLastLine(logPath);

    // Regex: YYYY-MM-DD HH:MM:SS [LEVEL] message
    std::regex tsRegex(R"(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2} \[DEBUG\] Test timestamp)");
    EXPECT_TRUE(std::regex_match(lastLine, tsRegex));
}

TEST(LoggerTest, ConsoleOutput) {
    // Redirect std::cout
    std::ostringstream oss;
    std::streambuf* oldCout = std::cout.rdbuf(oss.rdbuf());

    Logger& logger = Logger::instance();
    logger.setLevel(Logger::Level::DEBUG);
    logger.debug("Console test");

    std::cout.rdbuf(oldCout); // Restore cout

    std::string output = oss.str();
    EXPECT_TRUE(output.find("Console test") != std::string::npos);
}
