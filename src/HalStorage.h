#pragma once

#include <Print.h>
#include <Stream.h>
#include <common/FsApiConstants.h> // for oflag_t
#include <freertos/semphr.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "UsbDriveIo.h"
#include "WString.h"

class HalFile;

enum class UsbDriveState : uint8_t {
  Unsupported,
  WaitingForHost,
  Connected,
  Accessed,
  Ejected,
  Disconnected,
  IoError,
};

class HalStorage {
public:
  // A book cache file the reader cannot rebuild (progress, per-book settings
  // and stats). Mirrors the firmware helper exactly.
  static bool isBookUserData(const char *name) {
    const size_t len = strlen(name);
    const auto endsWith = [&](const char *suffix) {
      const size_t n = strlen(suffix);
      return len >= n && strcmp(name + len - n, suffix) == 0;
    };
    return strcmp(name, "progress.bin") == 0 ||
           strcmp(name, "progress.bin.bak") == 0 ||
           strcmp(name, "progress_percent.bin") == 0 ||
           strcmp(name, "reader_settings.bin") == 0 ||
           strcmp(name, "dictionary_history.txt") == 0 ||
           strcmp(name, "reading_stats_off") == 0 ||
           (strncmp(name, "stats", 5) == 0 &&
            (endsWith(".bin") || endsWith(".bin.bak")));
  }

  HalStorage();
  bool begin();
  bool ready() const;
  // No-op: the simulator's storage backend is POSIX fds with nothing to
  // power down before a simulated deep sleep.
  void shutdown();
  // Capacity of the filesystem holding the simulated SD root.
  uint64_t totalBytes() const;
  uint64_t usedBytes();
  std::vector<String> listFiles(const char *path = "/", int maxFiles = 200);
  // Read the entire file at `path` into a String. Returns empty string on
  // failure.
  String readFile(const char *path);
  // Low-memory helpers:
  // Stream the file contents to a `Print` (e.g. `Serial`, or any
  // `Print`-derived object). Returns true on success, false on failure.
  bool readFileToStream(const char *path, Print &out, size_t chunkSize = 256);
  // Read up to `bufferSize-1` bytes into `buffer`, null-terminating it. Returns
  // bytes read.
  size_t readFileToBuffer(const char *path, char *buffer, size_t bufferSize,
                          size_t maxBytes = 0);
  // Write a string to `path` on the SD card. Overwrites existing file.
  // Returns true on success.
  bool writeFile(const char *path, const String &content);
  // Ensure a directory exists, creating it if necessary. Returns true on
  // success.
  bool ensureDirectoryExists(const char *path);
  using UtcOffsetFn = uint8_t (*)(uint16_t year, uint8_t month, uint8_t day,
                                  uint8_t hour, uint8_t minute);
  // Host files already carry real timestamps; nothing to install.
  void installDateTimeCallback(UtcOffsetFn utcOffsetQuarterHoursAt);

  // USB Drive is never available in the simulator.
  bool beginUsbDrive();
  bool disconnectUsbDriveHost();
  void endUsbDrive();
  UsbDriveState usbDriveState() const;
  bool usbDriveHostSuspended() const;
  bool usbDriveIo(UsbDriveIo &out) const;

  HalFile open(const char *path, const oflag_t oflag = O_RDONLY);
  bool mkdir(const char *path, const bool pFlag = true);
  bool exists(const char *path);
  bool remove(const char *path);
  bool rename(const char *oldPath, const char *newPath);
  bool rmdir(const char *path);

  bool openFileForRead(const char *moduleName, const char *path, HalFile &file);
  bool openFileForRead(const char *moduleName, const std::string &path,
                       HalFile &file);
  bool openFileForRead(const char *moduleName, const String &path,
                       HalFile &file);
  // Same, for probes where a missing file is expected: no failure log.
  bool openFileForReadIfPresent(const char *moduleName, const char *path,
                                HalFile &file);
  bool openFileForReadIfPresent(const char *moduleName,
                                const std::string &path, HalFile &file);
  bool openFileForWrite(const char *moduleName, const char *path,
                        HalFile &file);
  bool openFileForWrite(const char *moduleName, const std::string &path,
                        HalFile &file);
  bool openFileForWrite(const char *moduleName, const String &path,
                        HalFile &file);
  bool removeDir(const char *path);

  // Library change tracking, as in the firmware. Every launch counts as a
  // fresh mount, so the first Library visit rescans ./fs_ (files may have
  // been dropped in from the host while the simulator was closed).
  uint32_t libraryContentGeneration() const {
    return libraryGeneration.load(std::memory_order_acquire);
  }
  void markLibraryContentChanged(const char *reason = nullptr);
  void noteLibraryScanned(uint32_t generation);
  bool libraryScanCurrent() const;

  static HalStorage &getInstance() { return instance; }

  class StorageLock; // private class, used internally

private:
  static HalStorage instance;

  bool initialized = false;
  SemaphoreHandle_t storageMutex = nullptr;
  std::atomic<uint32_t> libraryGeneration{0};
  std::atomic<bool> libraryScanned{false};
  std::atomic<uint32_t> libraryScannedGeneration{0};
};

#define Storage HalStorage::getInstance()

class HalFile : public Print {
  friend class HalStorage;
  class Impl;
  std::unique_ptr<Impl> impl;
  explicit HalFile(std::unique_ptr<Impl> impl);

public:
  HalFile();
  ~HalFile();
  HalFile(HalFile &&);
  HalFile &operator=(HalFile &&);
  HalFile(const HalFile &) = delete;
  HalFile &operator=(const HalFile &) = delete;

  void flush() override;
  size_t getName(char *name, size_t len);
  size_t size();
  size_t fileSize();
  uint64_t fileSize64();
  uint32_t creationTime();
  uint32_t modificationTime();
  bool seek(size_t pos);
  bool seek64(uint64_t pos);
  bool seekCur(int64_t offset);
  bool seekSet(size_t offset);
  int available() const;
  size_t position() const;
  int read(void *buf, size_t count);
  int read(); // read a single byte
  size_t write(const void *buf, size_t count);
  size_t write(const uint8_t *buf, size_t count) override;
  size_t write(uint8_t b) override;
  bool sync();
  bool rename(const char *newPath);
  bool isDirectory() const;
  void rewindDirectory();
  bool close();
  HalFile openNextFile();
  // The host filesystem does not model the device wrapper-allocation failure.
  bool allocationFailed() const { return false; }
  // readdir() failures are not distinguished from end-of-directory here.
  bool iterationFailed() const { return false; }
  bool isOpen() const;
  operator bool() const;
};

// Only do renaming FsFile to HalFile if this header is included by downstream
// code The renaming is to allow using the thread-safe HalFile instead of the
// raw FsFile, without needing to change the downstream code
#ifndef HAL_STORAGE_IMPL
using FsFile = HalFile;
#endif

// Downstream code must use Storage instead of SdMan
#ifdef SdMan
#undef SdMan
#endif
