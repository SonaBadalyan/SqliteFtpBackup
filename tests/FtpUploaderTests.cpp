#include "FtpUploader.h"
#include "Logger.h"
#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <sstream>

class FtpUploaderTest : public ::testing::Test {
protected:
    std::string testFile = "test_upload.txt";

    void SetUp() override {
        // Create a dummy file to upload
        std::ofstream ofs(testFile);
        ofs << "dummy content";
    }

    void TearDown() override {
        // Remove dummy file
        if (std::filesystem::exists(testFile))
            std::filesystem::remove(testFile);
    }
};

// Test that URL is built correctly
TEST_F(FtpUploaderTest, BuildUrlCorrectly) {
    FtpUploader uploader("127.0.0.1", 21, "user", "pass");
    std::string url = uploader.buildUrl("dir/subdir", "file.txt");
    EXPECT_EQ(url, "ftp://127.0.0.1:21/dir/subdir/file.txt");

    // Test backslashes
    url = uploader.buildUrl("dir\\subdir\\", "file.txt");
    EXPECT_EQ(url, "ftp://127.0.0.1:21/dir/subdir/file.txt");
}

// Test that uploading non-existent file throws
TEST_F(FtpUploaderTest, UploadNonExistentFileThrows) {
    FtpUploader uploader("127.0.0.1", 21, "user", "pass");
    EXPECT_THROW(uploader.uploadFile("nonexistent.txt", "remote"), std::runtime_error);
}

// Test that uploading existing file calls curl and logs error if fails
TEST_F(FtpUploaderTest, UploadFileLogsErrorOnCurlFail) {
    FtpUploader uploader("127.0.0.1", 21, "user", "pass");
    uploader.setRetries(1); // keep retry count low

    // Capture console logs
    std::ostringstream oss;
    auto& logger = Logger::instance();
    std::streambuf* oldCout = std::cout.rdbuf(oss.rdbuf());

    EXPECT_THROW(uploader.uploadFile(testFile, "remote"), std::runtime_error);

    std::cout.rdbuf(oldCout); // restore cout

    std::string logOutput = oss.str();
    EXPECT_TRUE(logOutput.find("FTP upload failed") != std::string::npos);
}

// Test setting timeout and retries
TEST_F(FtpUploaderTest, SetTimeoutAndRetries) {
    FtpUploader uploader("127.0.0.1", 21, "user", "pass");
    uploader.setTimeout(5);
    uploader.setRetries(3);

    // We can only indirectly test by checking internal getters if added
    // For now, just ensure these calls do not throw
    SUCCEED();
}
