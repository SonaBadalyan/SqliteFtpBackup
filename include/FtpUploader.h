#pragma once
#include <string>
#include <functional>

/**
 * @brief Simple FTP uploader using libcurl
 *
 * Supports uploading files to an FTP server with
 * timeouts, retries, progress callbacks, SSL verification,
 * and verbose logging.
 */
class FtpUploader {
public:
    using ProgressCallback = std::function<void(double dltotal, double dlnow,
                                                double ultotal, double ulnow)>;

    FtpUploader(const std::string& host, int port,
                const std::string& user, const std::string& pass);

    ~FtpUploader();

    /**
     * @brief Upload a local file to the remote directory on the FTP server
     * @param localFile Full path to the local file
     * @param remoteDir Directory on server
     * @throws std::runtime_error on failure
     */
    void uploadFile(const std::string& localFile, const std::string& remoteDir);

    // Optional features
    void setTimeout(long seconds);                  // Connection & read timeout (default 30s)
    void setRetries(int count);                     // Retry failed uploads (default 3)
    void enableVerbose(bool verbose = true);        // Verbose logging
    void setProgressCallback(ProgressCallback cb);  // Progress tracking
    void setSslVerify(bool enable);                 // Enable/disable SSL verification (default true)
    std::string getLastError() const;               // Last error message

    /**
     * @brief Build a properly formatted FTP URL
     * @param remoteDir Remote directory on server
     * @param filename File name to append
     * @return Full FTP URL
     */
    std::string buildUrl(const std::string& remoteDir,
                         const std::string& filename) const;

private:
    std::string host;
    int port;
    std::string user;
    std::string pass;

    long timeoutSeconds = 30;
    int maxRetries = 3;
    bool verbose = false;
    bool sslVerify = true;

    ProgressCallback progressCb;
    std::string lastError;

    // Internal helpers
    void throwIfFailed(int attempt, const std::string& context);
};
