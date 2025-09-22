#include "FtpUploader.h"
#include "Logger.h"
#include <curl/curl.h>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <memory>
#include <algorithm>
#include <thread>
#include <chrono>


namespace {
    // RAII wrapper for FILE*
    struct FileCloser { void operator()(FILE* fp) const { if (fp) fclose(fp); } };
    using FilePtr = std::unique_ptr<FILE, FileCloser>;

    // Curl progress callback (newer xferinfo signature)
    int curlProgress(void* clientp,
                     curl_off_t dltotal, curl_off_t dlnow,
                     curl_off_t ultotal, curl_off_t ulnow) {
        auto* cb = reinterpret_cast<FtpUploader::ProgressCallback*>(clientp);
        if (cb && *cb) {
            try {
                (*cb)(static_cast<double>(dltotal), static_cast<double>(dlnow),
                      static_cast<double>(ultotal), static_cast<double>(ulnow));
            } catch (...) {
                // never throw from callback into curl
                return 1; // signal abort
            }
        }
        return 0;
    }

    // Write callback to log server response line by line
    size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
        size_t totalSize = size * nmemb;
        std::string chunk(ptr, totalSize);
        std::stringstream ss(chunk);
        std::string line;
        while (std::getline(ss, line)) {
            if (!line.empty()) {
                Logger::instance().info("FTP server: " + line);
            }
        }
        return totalSize;
    }

    // Helper to sleep for backoff
    void sleepForBackoff(int attempt) {
        using namespace std::chrono_literals;
        // Exponential backoff: base 500ms * 2^(attempt-1)
        int64_t ms = 500LL * (1LL << (std::min(attempt - 1, 6))); // cap exponent so we don't overflow
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
}

FtpUploader::FtpUploader(const std::string& host, int port,
                         const std::string& user, const std::string& pass)
    : host(host), port(port), user(user), pass(pass),
      timeoutSeconds(30), maxRetries(1), verbose(false), progressCb(nullptr),
      lastError(), sslVerify(true)
{
    // Initialize libcurl globally once per process. curl_global_init is safe to call multiple times,
    // but should have matching cleanup — we will call global init here and rely on cleanup in destructor.
    CURLcode c = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (c != CURLE_OK) {
        Logger::instance().warn("curl_global_init failed, continuing but curl may misbehave.");
    }
    Logger::instance().info("FtpUploader initialized for host: " + host);
}

FtpUploader::~FtpUploader() {
    Logger::instance().info("FtpUploader destroyed for host: " + host);
    // Best-effort cleanup
    curl_global_cleanup();
}

void FtpUploader::setTimeout(long seconds) { timeoutSeconds = seconds; }
void FtpUploader::setRetries(int count) { maxRetries = std::max(1, count); }
void FtpUploader::enableVerbose(bool v) { verbose = v; }
void FtpUploader::setProgressCallback(ProgressCallback cb) { progressCb = cb; }
std::string FtpUploader::getLastError() const { return lastError; }
void FtpUploader::setSslVerify(bool v) { sslVerify = v; }

std::string FtpUploader::buildUrl(const std::string& remoteDir,
                                  const std::string& filename) const {
    std::string cleanedDir = remoteDir;
    std::replace(cleanedDir.begin(), cleanedDir.end(), '\\', '/');
    // Remove leading or trailing slashes to avoid double slashes
    while (!cleanedDir.empty() && cleanedDir.front() == '/') cleanedDir.erase(cleanedDir.begin());
    while (!cleanedDir.empty() && cleanedDir.back() == '/') cleanedDir.pop_back();

    std::ostringstream url;
    url << "ftp://" << host;
    if (port > 0) url << ":" << port;
    if (!cleanedDir.empty()) url << "/" << cleanedDir;
    url << "/" << filename;
    return url.str();
}

void FtpUploader::throwIfFailed(int attempt, const std::string& context) {
    if (!lastError.empty() && attempt >= maxRetries) {
        Logger::instance().error(context + ": " + lastError);
        throw std::runtime_error(context + ": " + lastError);
    }
}

void FtpUploader::uploadFile(const std::string& localFile,
                             const std::string& remoteDir) {
    Logger::instance().info("Preparing to upload file: " + localFile + " to " + remoteDir);

    if (!std::filesystem::exists(localFile)) {
        Logger::instance().error("Local file does not exist: " + localFile);
        throw std::runtime_error("Local file does not exist: " + localFile);
    }

    std::filesystem::path p(localFile);
    std::string filename = p.filename().string();
    std::string url = buildUrl(remoteDir, filename);

    int attempt = 0;
    lastError.clear();

    while (attempt < maxRetries) {
        ++attempt;
        Logger::instance().info("FTP upload attempt " + std::to_string(attempt) + " to URL: " + url);

        FilePtr fp(fopen(localFile.c_str(), "rb"));
        if (!fp) {
            Logger::instance().error("Failed to open local file: " + localFile);
            throw std::runtime_error("Failed to open local file: " + localFile);
        }

        CURL* curl = curl_easy_init();
        if (!curl) {
            Logger::instance().error("Failed to initialize curl");
            throw std::runtime_error("Failed to initialize curl");
        }

        // Always set URL and authentication
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        if (!user.empty()) curl_easy_setopt(curl, CURLOPT_USERNAME, user.c_str());
        if (!pass.empty()) curl_easy_setopt(curl, CURLOPT_PASSWORD, pass.c_str());

        // Use SSL for FTP if available; allow toggling verification
        curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, sslVerify ? 1L : 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, sslVerify ? 2L : 0L);

        // Upload settings
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(curl, CURLOPT_READDATA, fp.get());

        // Create missing directories on server if curl supports it
        curl_easy_setopt(curl, CURLOPT_FTP_CREATE_MISSING_DIRS, CURLFTP_CREATE_DIR_RETRY);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, timeoutSeconds);
        curl_easy_setopt(curl, CURLOPT_FTP_RESPONSE_TIMEOUT, timeoutSeconds);

        // Write function (server replies) and verbose
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        if (verbose) curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

        // Progress callback
        if (progressCb) {
            // CURLOPT_XFERINFOFUNCTION requires CURLOPT_NOPROGRESS 0L
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curlProgress);
            curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progressCb);
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        } else {
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
        }

        // Important: set read size (optional) and file size for progress calculation
        try {
            std::uintmax_t filesize = std::filesystem::file_size(localFile);
            curl_off_t fsize = static_cast<curl_off_t>(filesize);
            curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, fsize);
        } catch (...) {
            // If we cannot determine file size, proceed without it; progress will be less precise.
        }

        lastError.clear();
        CURLcode res = curl_easy_perform(curl);
        long responseCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
        curl_easy_cleanup(curl);

        if (res == CURLE_OK) {
            Logger::instance().info("FTP upload succeeded: " + filename);
            return; // success, exit the function
        } else {
            lastError = curl_easy_strerror(res);
            Logger::instance().warn("FTP upload attempt " + std::to_string(attempt) + " failed: " + lastError);
            if (attempt < maxRetries) {
                Logger::instance().info("Retrying after backoff...");
                sleepForBackoff(attempt);
            }
        }
    }

    // If we reach here, all attempts failed
    Logger::instance().error("FTP upload failed after " + std::to_string(maxRetries) + " attempts: " + lastError);
    throw std::runtime_error("FTP upload failed: " + lastError);
}
