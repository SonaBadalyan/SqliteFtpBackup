#include "SqliteHelper.h"
#include "FtpUploader.h"
#include "Logger.h"
#include <iostream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>
#include <cstdlib>
#include <map>

// Exit codes
constexpr int EXIT_INVALID_ARGS   = 1;
constexpr int EXIT_UPLOAD_FAILED  = 2;
constexpr int EXIT_CONFIG_ERROR   = 3;

// Utility to get current timestamp string
std::string currentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now;
#if defined(_WIN32)
    localtime_s(&tm_now, &t);
#else
    localtime_r(&t, &tm_now);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_now, "%Y-%m-%d_%H-%M-%S");
    return oss.str();
}

// Print usage instructions
void printUsage(const std::string& exeName) {
    std::cerr << "Usage:\n"
              << "  " << exeName
              << " <sqlite_prefix> <ftp_host> <ftp_port> <ftp_user> <ftp_pass_or_-> <ftp_dir>\n"
              << "Options:\n"
              << "  --no-ssl-verify        Disable SSL peer/host verification (default: enabled)\n"
              << "  --rows N               Number of rows to insert into DB (default: 100)\n"
              << "  --retries N            FTP retries on failure (default: 3)\n"
              << "  --timeout SECONDS      FTP connection & response timeout (default: 30)\n"
              << "  --log-level LEVEL      Set log level: debug|info|warn|error (default: info)\n"
              << "\n"
              << "Notes:\n"
              << "  - If <ftp_pass_or_-> is '-', password will be read from FTP_PASS environment variable.\n"
              << "  - Exit codes: "
              << EXIT_INVALID_ARGS << " (bad args), "
              << EXIT_UPLOAD_FAILED << " (upload failed), "
              << EXIT_CONFIG_ERROR << " (config error)\n";
}

// RAII helper to ensure temporary file is removed if created
class TempFileRemover {
public:
    explicit TempFileRemover(const std::filesystem::path& p) : path_(p), active_(true) {}
    ~TempFileRemover() {
        if (active_ && !path_.empty()) {
            try {
                std::filesystem::remove(path_);
                Logger::instance().info("Temporary file removed: " + path_.string());
            } catch (...) {
                // Avoid throwing in destructor
            }
        }
    }
private:
    std::filesystem::path path_;
    bool active_;
};

// Backup orchestrator
class BackupManager {
public:
    BackupManager(const std::string& sqlitePrefix,
                  const std::string& ftpHost, int ftpPort,
                  const std::string& ftpUser, const std::string& ftpPass,
                  const std::string& ftpDir,
                  bool sslVerify, int rows, int retries, long timeout)
        : sqlitePrefix(sqlitePrefix), ftpHost(ftpHost), ftpPort(ftpPort),
          ftpUser(ftpUser), ftpPass(ftpPass), ftpDir(ftpDir),
          sslVerify(sslVerify), rows(rows), retries(retries), timeout(timeout) {}

    bool run() {
        Logger& log = Logger::instance();
        try {
            std::filesystem::create_directories("logs");

            log.setLevel(logLevel);

            SqliteHelper db(sqlitePrefix);
            db.createTable();
            db.insertRandomRows(rows);
            log.info("Total rows after insert: " + std::to_string(db.getRowCount()));

            std::string dumpFile = sqlitePrefix + "_backup_" + currentTimestamp() + ".sqlite";
            TempFileRemover remover(dumpFile);

            db.backupToFile(dumpFile);
            log.info("Database binary backup created at: " + dumpFile);

            FtpUploader uploader(ftpHost, ftpPort, ftpUser, ftpPass);
            uploader.enableVerbose(true);
            uploader.setRetries(retries);
            uploader.setTimeout(timeout);
            uploader.setSslVerify(sslVerify);

            uploader.setProgressCallback([](double, double, double ultotal, double ulnow) {
                if (ultotal > 0) {
                    int percent = static_cast<int>((ulnow / ultotal) * 100.0);
                    Logger::instance().debug("Upload progress: " + std::to_string(percent) + "%");
                }
            });

            log.info("Starting upload to directory: " + ftpDir);
            uploader.uploadFile(dumpFile, ftpDir);
            log.info("Upload finished successfully.");

        } catch (const std::exception& ex) {
            Logger::instance().error("Exception during backup/upload: " + std::string(ex.what()));
            return false;
        } catch (...) {
            Logger::instance().error("Unknown exception during backup/upload.");
            return false;
        }
        return true;
    }

    void setLogLevel(Logger::Level lvl) { logLevel = lvl; }

private:
    std::string sqlitePrefix;
    std::string ftpHost;
    int ftpPort;
    std::string ftpUser;
    std::string ftpPass;
    std::string ftpDir;
    bool sslVerify;
    int rows;
    int retries;
    long timeout;
    Logger::Level logLevel = Logger::Level::INFO;
};

// Helper: parse --flag=value or --flag value style
bool parseOptionalFlag(int& i, int argc, char** argv,
                       std::string_view& flagOut, std::string_view& valueOut) {
    std::string_view arg = argv[i];
    auto eqPos = arg.find('=');
    if (eqPos != std::string_view::npos) {
        flagOut = arg.substr(0, eqPos);
        valueOut = arg.substr(eqPos + 1);
        return true;
    } else if (i + 1 < argc) {
        flagOut = arg;
        valueOut = argv[++i];
        return true;
    }
    flagOut = arg;
    valueOut = {};
    return false;
}


int main(int argc, char** argv) {
    if (argc < 7) {
        std::cerr << "Invalid number of arguments.\n";
        printUsage(argv[0]);
        return EXIT_INVALID_ARGS;
    }

    std::string_view sqlitePrefix = argv[1];
    std::string_view ftpHost      = argv[2];

    int ftpPort = 0;
    try {
        ftpPort = std::stoi(argv[3]);
        if (ftpPort <= 0 || ftpPort > 65535) {
            throw std::out_of_range("FTP port out of valid range (1-65535)");
        }
    } catch (const std::exception& e) {
        std::cerr << "Invalid FTP port: " << argv[3] << " (" << e.what() << ")\n";
        printUsage(argv[0]);
        return EXIT_INVALID_ARGS;
    }

    std::string_view ftpUser = argv[4];
    std::string_view ftpPassArg = argv[5];
    std::string_view ftpDir  = argv[6];

    // Defaults
    bool sslVerify = false;
    int rows = 100;
    int retries = 3;
    long timeout = 30;
    Logger::Level logLevel = Logger::Level::INFO;

    for (int i = 7; i < argc; ++i) {
        std::string_view flag, value;
        if (!parseOptionalFlag(i, argc, argv, flag, value)) {
            std::cerr << "Invalid option format: " << argv[i] << "\n";
            return EXIT_INVALID_ARGS;
        }

        try {
            if (flag == "--no-ssl-verify") {
                sslVerify = false;
            } else if (flag == "--rows") {
                rows = std::stoi(std::string(value));
                if (rows <= 0) throw std::out_of_range("must be > 0");
            } else if (flag == "--retries") {
                retries = std::stoi(std::string(value));
                if (retries < 0) throw std::out_of_range("must be >= 0");
            } else if (flag == "--timeout") {
                timeout = std::stol(std::string(value));
                if (timeout <= 0) throw std::out_of_range("must be > 0");
            } else if (flag == "--log-level") {
                if (value == "debug") logLevel = Logger::Level::DEBUG;
                else if (value == "info") logLevel = Logger::Level::INFO;
                else if (value == "warn") logLevel = Logger::Level::WARNING;
                else if (value == "error") logLevel = Logger::Level::ERROR;
                else throw std::invalid_argument("invalid log level");
            } else {
                std::cerr << "Unknown option: " << flag << "\n";
                printUsage(argv[0]);
                return EXIT_INVALID_ARGS;
            }
        } catch (const std::exception& e) {
            std::cerr << "Invalid value for " << flag << ": " << e.what() << "\n";
            return EXIT_INVALID_ARGS;
        }

    }

    // Read password from environment if requested
    std::string ftpPass;
    if (ftpPassArg == "-") {
        const char* envPass = std::getenv("FTP_PASS");
        if (!envPass) {
            std::cerr << "Password argument '-' specified but FTP_PASS environment variable is not set.\n";
            return EXIT_CONFIG_ERROR;
        }
        ftpPass = envPass;
    } else {
        ftpPass = std::string(ftpPassArg);
    }

    // Masked log for credentials
    std::string maskedPass = ftpPass.empty() ? "<empty>" : std::string(ftpPass.size(), '*');
    Logger::instance().info("Starting backup. FTP host: " + std::string(ftpHost) + ":" + std::to_string(ftpPort)
                            + ", user: " + std::string(ftpUser) + ", pass: " + maskedPass);

    BackupManager mgr(std::string(sqlitePrefix), std::string(ftpHost), ftpPort,
                      std::string(ftpUser), ftpPass, std::string(ftpDir),
                      sslVerify, rows, retries, timeout);
    mgr.setLogLevel(logLevel);

    bool success = mgr.run();

    if (!success) {
        std::cerr << "Backup and upload failed. See logs for details.\n";
        return EXIT_UPLOAD_FAILED;
    }

    std::cout << "Backup and upload completed successfully.\n";
    return 0;
}
