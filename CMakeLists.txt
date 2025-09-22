cmake_minimum_required(VERSION 3.15)
project(SqliteFtpBackup VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTS "Build tests" ON)

include(FetchContent)

# -------------------------------
# SQLite
# -------------------------------
set(SQLITE3_INCLUDE_DIR "C:/msys64/ucrt64/include")
set(SQLITE3_LIBRARY     "C:/msys64/ucrt64/lib/libsqlite3.a")

if(NOT EXISTS ${SQLITE3_INCLUDE_DIR}/sqlite3.h OR NOT EXISTS ${SQLITE3_LIBRARY})
    message(FATAL_ERROR "SQLite not found at specified paths")
endif()

# -------------------------------
# OpenSSL
# -------------------------------
find_package(OpenSSL QUIET)

if(NOT OpenSSL_FOUND)
    message(STATUS "OpenSSL not found, fetching...")
    FetchContent_Declare(
        openssl
        URL https://www.openssl.org/source/openssl-3.2.0.tar.gz
    )
    FetchContent_MakeAvailable(openssl)
    set(OPENSSL_INCLUDE_DIR ${openssl_SOURCE_DIR}/include)
    set(OPENSSL_LIBRARIES ${openssl_BINARY_DIR}/lib/libssl.a ${openssl_BINARY_DIR}/lib/libcrypto.a)
endif()

# -------------------------------
# Curl
# -------------------------------
find_package(CURL QUIET)

if(NOT CURL_FOUND)
    message(STATUS "CURL not found, fetching...")
    FetchContent_Declare(
        curl
        URL https://curl.se/download/curl-8.16.0.zip
    )
    # Disable optional dependencies
    set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
    set(CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)
    set(CURL_USE_NGHTTP2 OFF CACHE BOOL "" FORCE)
    set(CURL_USE_BROTLI OFF CACHE BOOL "" FORCE)
    set(CURL_USE_LIBIDN2 OFF CACHE BOOL "" FORCE)
    set(CURL_USE_LIBSSH2 OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

    
    set(CURL_STATICLIB ON CACHE BOOL "" FORCE)
    set(CURL_USE_OPENSSL ON CACHE BOOL "" FORCE)  # Use OpenSSL

    FetchContent_MakeAvailable(curl)

    set(CURL_INCLUDE_DIRS ${curl_SOURCE_DIR}/include)
    set(CURL_LIB_TARGET libcurl_object)

endif()

# -------------------------------
# Core library
# -------------------------------
add_library(SqliteFtpBackupLib STATIC
    src/FtpUploader.cpp
    src/SqliteHelper.cpp
)

target_include_directories(SqliteFtpBackupLib PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${SQLITE3_INCLUDE_DIR}
    ${CURL_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
)

target_compile_definitions(SqliteFtpBackupLib PRIVATE CURL_STATICLIB)

target_link_libraries(SqliteFtpBackupLib PRIVATE
    ${CURL_LIB_TARGET}
    ${SQLITE3_LIBRARY}
    ${OPENSSL_LIBRARIES}
    ws2_32
    wldap32
    crypt32
)

# -------------------------------
# Executable
# -------------------------------
add_executable(SqliteFtpBackup console/main.cpp)
target_link_libraries(SqliteFtpBackup PRIVATE SqliteFtpBackupLib)

# -------------------------------
# Tests
# -------------------------------

if(BUILD_TESTS)
    # Fetch GoogleTest
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()
    include(GoogleTest)

    # ---------------------------
    # SqliteHelperTests
    # ---------------------------
    add_executable(SqliteHelperTests tests/SqliteHelperTests.cpp)
    target_include_directories(SqliteHelperTests PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(SqliteHelperTests PRIVATE SqliteFtpBackupLib gtest gtest_main)
    target_compile_definitions(SqliteHelperTests PRIVATE CURL_STATICLIB)
    gtest_discover_tests(SqliteHelperTests)

    # ---------------------------
    # LoggerTests
    # ---------------------------
    add_executable(LoggerTests tests/LoggerTests.cpp)
    target_include_directories(LoggerTests PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(LoggerTests PRIVATE SqliteFtpBackupLib gtest gtest_main)
    target_compile_definitions(LoggerTests PRIVATE CURL_STATICLIB)
    gtest_discover_tests(LoggerTests)

    # ---------------------------
    # FtpUploaderTests
    # ---------------------------
    add_executable(FtpUploaderTests tests/FtpUploaderTests.cpp)
    target_include_directories(FtpUploaderTests PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(FtpUploaderTests PRIVATE SqliteFtpBackupLib gtest gtest_main)
    target_compile_definitions(FtpUploaderTests PRIVATE CURL_STATICLIB)
    gtest_discover_tests(FtpUploaderTests)
endif()

