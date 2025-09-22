
# SqliteFtpBackup

## Overview
`SqliteFtpBackup` is a **C++17 console application** for creating SQLite database backups, populating them with sample data, dumping the database to a SQLite file, and uploading the dump to an FTP server.  

It includes:  
- **SQLite database management** via `SqliteHelper`  
- **FTP file upload** via `FtpUploader` (libcurl + OpenSSL)  
- **Thread-safe logging** via `Logger`  
- **Unit tests** using GoogleTest  

---

## Features

- Creates a `people` table with sample data  
- Inserts a configurable number of random rows (default: 100)  
- Dumps database to a **timestamped SQLite file**  
- Uploads the dump file to a specified FTP server and directory  
- Configurable FTP **retries**, **timeout**, and **SSL verification**  
- Thread-safe logging to console and file (`logs/` directory)  
- Supports **log levels**: `debug`, `info`, `warn`, `error`  
- Temporary dump files are **automatically cleaned up** after upload  
- Exit codes for robust error handling  

---

## Directory Structure

```
SqliteFtpBackup/
│
├─ include/                 
│  ├─ SqliteHelper.h
│  ├─ FtpUploader.h
│  └─ Logger.h
│
├─ src/                     
│  ├─ SqliteHelper.cpp
│  └─ FtpUploader.cpp
│
├─ console/
│  └─ main.cpp              
│
├─ tests/                   
│  ├─ CMakeLists.txt
│  ├─ SqliteHelperTests.cpp
│  ├─ LoggerTests.cpp
│  └─ FtpUploaderTests.cpp
│
├─ CMakeLists.txt           
└─ README.md
```

---

## Prerequisites

- **CMake ≥ 3.15**  
- **C++17 compiler**  
- **Optional system dependencies** (auto-fetched if missing):  
  - SQLite3  
  - libcurl (built with OpenSSL)  
  - OpenSSL  
- **Windows-specific libraries:** `ws2_32`, `wldap32`, `crypt32`  

---

## Build Instructions

1. **Create a build directory**
```bash
mkdir build
cd build
```

2. **Configure with CMake**
```bash
cmake .. -DBUILD_TESTS=ON
```
> Missing dependencies (OpenSSL, libcurl, GoogleTest) will be automatically downloaded and built.

3. **Build project**
```bash
cmake --build . --config Release
```
- Generates:
  - `SqliteFtpBackupLib` (static library)  
  - `SqliteFtpBackup` executable  
  - Optional test executables if `BUILD_TESTS=ON`  

---

## Running the Application

```bash
SqliteFtpBackup <sqlite_prefix> <ftp_host> <ftp_port> <ftp_user> <ftp_pass_or_-> <ftp_dir> [options]
``

**Example:**
```bash
SqliteFtpBackup C:\DBackup 127.0.0.1 21 user - FTP --rows 200 --retries 5 --timeout 60 --log-level debug
```

### Options

| Flag                 | Description |
|---------------------|-------------|
| `--no-ssl-verify`    | Disable SSL peer/host verification (default: enabled) |
| `--rows N`           | Number of rows to insert into DB (default: 100) |
| `--retries N`        | FTP retries on failure (default: 3) |
| `--timeout SECONDS`  | FTP connection & response timeout (default: 30) |
| `--log-level LEVEL`  | Set log level: `debug`|`info`|`warn`|`error` (default: `info`) |

**Notes:**  
- If `<ftp_pass_or_->` is `-`, the password will be read from the `FTP_PASS` environment variable.  
- Temporary dump files are **automatically deleted** after successful upload.  

---

## Logs

- Stored in the `logs/` directory with timestamps  
- Levels: `DEBUG`, `INFO`, `WARNING`, `ERROR`  
- Example to set minimum logging level:
```cpp
Logger::instance().setLevel(Logger::Level::INFO);
```

---

## Generated Files & Folders

- `logs/` → Log files per run  
- `<sqlite_prefix>_backup_<timestamp>.sqlite` → Temporary SQLite backup  
- `build/` → CMake build artifacts  
- `*.sqlite` → Timestamped SQLite database backups  

---

## Exit Codes

| Code | Meaning |
|------|---------|
| `1`  | Invalid arguments |
| `2`  | Backup/upload failed |
| `3`  | Configuration error (e.g., invalid log level, missing FTP_PASS) |

---

## Tests

- Built with **GoogleTest** and run via **CTest**  
- Run all tests:
```bash
ctest --output-on-failure
```
- Test executables:  
  - `SqliteHelperTests`  
  - `LoggerTests`  
  - `FtpUploaderTests`  

---

## Dependencies

- **SQLite3** → `sqlite3.h` + static lib  
- **libcurl** → Built with OpenSSL for secure FTP  
- **OpenSSL** → Required for FTP over TLS/SSL  
- **GoogleTest** → Unit testing framework  

---

## Notes & Tips

- Logger outputs color-coded console messages and file logs  
- FTP upload supports retries, configurable timeout, and SSL verification  
- Temporary files are **safely cleaned up** even if an exception occurs  
- Recommended workflow: create backup → verify → upload → cleanup  
