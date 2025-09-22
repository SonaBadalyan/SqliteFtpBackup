#pragma once
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <string>
#include <stdexcept>

class Logger {
public:
    enum class Level { DEBUG, INFO, WARNING, ERROR };

    // Singleton instance
    static Logger& instance() {
        static Logger logger;
        return logger;
    }

    // Set minimum log level
    void setLevel(Level lvl) {
        std::lock_guard<std::mutex> lock(mtx_);
        minLevel_ = lvl;
    }

    // Optional: enable log rotation (max file size in bytes)
    void setMaxFileSize(std::size_t bytes) {
        maxFileSize_ = bytes;
    }

    // Variadic logging
    template<typename... Args>
    void debug(Args&&... args) { log(Level::DEBUG, std::forward<Args>(args)...); }

    template<typename... Args>
    void info(Args&&... args) { log(Level::INFO, std::forward<Args>(args)...); }

    template<typename... Args>
    void warn(Args&&... args) { log(Level::WARNING, std::forward<Args>(args)...); }

    template<typename... Args>
    void error(Args&&... args) { log(Level::ERROR, std::forward<Args>(args)...); }

private:
    std::ofstream file_;
    Level minLevel_ = Level::DEBUG;
    std::mutex mtx_;
    std::size_t maxFileSize_ = 0; // 0 = no rotation

    Logger() {
        try {
            std::filesystem::create_directories("logs");
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Failed to create logs directory: " << e.what() << "\n";
        }
        openNewLogFile();
    }

    ~Logger() {
        if (file_.is_open()) file_.close();
    }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // -----------------------
    // Variadic template log
    // -----------------------
    template<typename... Args>
    void log(Level lvl, Args&&... args) {
        if (lvl < minLevel_) return;

        std::lock_guard<std::mutex> lock(mtx_);

        // Concatenate all arguments
        std::ostringstream oss;
        (oss << ... << args);

        std::string message = timestamp() + " [" + levelToString(lvl) + "] " + oss.str() + "\n";

        // Console output
        std::cout << levelToColor(lvl) << message << "\033[0m";

        // File output
        if (file_.is_open()) {
            file_ << message;
            file_.flush();

            // Log rotation
            if (maxFileSize_ > 0 && file_.tellp() >= static_cast<std::streampos>(maxFileSize_)) {
                file_.close();
                openNewLogFile();
            }
        }
    }

    // -----------------------
    // Helpers
    // -----------------------
    static std::string timestamp() {
        using namespace std::chrono;
        auto now = system_clock::now();
        auto t = system_clock::to_time_t(now);
        std::tm tm_now;
#if defined(_WIN32)
        localtime_s(&tm_now, &t);
#else
        localtime_r(&t, &tm_now);
#endif
        std::ostringstream oss;
        oss << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    static std::string levelToString(Level lvl) {
        switch (lvl) {
            case Level::DEBUG:   return "DEBUG";
            case Level::INFO:    return "INFO";
            case Level::WARNING: return "WARNING";
            case Level::ERROR:   return "ERROR";
        }
        return "UNKNOWN";
    }

    static std::string levelToColor(Level lvl) {
        switch (lvl) {
            case Level::DEBUG:   return "\033[36m"; // cyan
            case Level::INFO:    return "\033[32m"; // green
            case Level::WARNING: return "\033[33m"; // yellow
            case Level::ERROR:   return "\033[31m"; // red
        }
        return "\033[0m";
    }

    void openNewLogFile() {
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        std::tm tm_now;
#if defined(_WIN32)
        localtime_s(&tm_now, &t);
#else
        localtime_r(&t, &tm_now);
#endif

        std::ostringstream oss;
        oss << "logs/app_" << std::put_time(&tm_now, "%Y-%m-%d_%H-%M-%S") << ".log";
        file_.open(oss.str(), std::ios::out);

        if (!file_.is_open()) {
            std::cerr << "Failed to create log file: " << oss.str() << "\n";
        }
    }
};
