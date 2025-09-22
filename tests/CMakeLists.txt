# tests/CMakeLists.txt
cmake_minimum_required(VERSION 3.15)
project(SqliteFtpBackupTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------
# GoogleTest
# ---------------------------
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/release-1.14.0.zip
)
FetchContent_MakeAvailable(googletest)

enable_testing()
include(GoogleTest)

# ---------------------------
# Test executables
# ---------------------------

# SqliteHelperTests
add_executable(SqliteHelperTests
    SqliteHelperTests.cpp
)
target_link_libraries(SqliteHelperTests
    PRIVATE
        SqliteFtpBackupLib
        GTest::gtest_main
)
gtest_discover_tests(SqliteHelperTests)

# LoggerTests
add_executable(LoggerTests
    LoggerTests.cpp
)
target_link_libraries(LoggerTests
    PRIVATE
        SqliteFtpBackupLib
        GTest::gtest_main
)
gtest_discover_tests(LoggerTests)

# FtpUploaderTests
add_executable(FtpUploaderTests
    FtpUploaderTests.cpp
)
target_link_libraries(FtpUploaderTests
    PRIVATE
        SqliteFtpBackupLib
        GTest::gtest_main
)
gtest_discover_tests(FtpUploaderTests)

# ctest --output-on-failure