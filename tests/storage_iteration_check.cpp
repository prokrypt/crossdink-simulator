#include <HalStorage.h>
#include <dirent.h>

#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <string>
#include <sys/statvfs.h>
#include <unistd.h>

// Only HalStorage.cpp is compiled with readdir redirected here. Exercise the
// real iterator against a deterministic POSIX I/O error, not an always-false
// shim.
static bool failNextRead = false;
static bool failNextOpenDirectory = false;
extern "C" DIR *simulatorTestOpenDirectory(const char *path) {
  if (failNextOpenDirectory) {
    failNextOpenDirectory = false;
    errno = EACCES;
    return nullptr;
  }
  return opendir(path);
}
extern "C" struct dirent *simulatorTestReadDirectory(DIR *dir) {
  if (failNextRead) {
    failNextRead = false;
    errno = EIO;
    return nullptr;
  }
  return readdir(dir);
}

int main(int argc, char **argv) {
  assert(argc == 2);
  assert(setenv("CROSSPOINT_SIM_SD", argv[1], 1) == 0);
  assert(Storage.begin());
  struct statvfs capacity{};
  assert(statvfs(argv[1], &capacity) == 0);
  assert(Storage.totalBytes() ==
         uint64_t(capacity.f_blocks) * capacity.f_frsize);
  assert(Storage.usedBytes() <= Storage.totalBytes());
  const auto missingRoot = std::string(argv[1]) + "/missing-capacity-root";
  assert(setenv("CROSSPOINT_SIM_SD", missingRoot.c_str(), 1) == 0);
  assert(Storage.totalBytes() == 0 && Storage.usedBytes() == UINT64_MAX);
  assert(setenv("CROSSPOINT_SIM_SD", argv[1], 1) == 0);
  assert(Storage.mkdir("/empty"));
  auto directory = Storage.open("/empty");
  assert(directory.isDirectory());
  errno = EIO; // A stale errno must not turn ordinary EOF into an error.
  assert(!directory.openNextFile());
  assert(!directory.iterationFailed());
  failNextRead = true;
  assert(!directory.openNextFile());
  assert(directory.iterationFailed());
  directory.rewindDirectory();
  assert(!directory.iterationFailed());
  assert(!directory.openNextFile());
  assert(!directory.iterationFailed());
  failNextRead = true;
  assert(!directory.openNextFile());
  assert(directory.iterationFailed());
  auto moved = std::move(directory);
  assert(moved.iterationFailed());
  assert(!directory.iterationFailed());
  moved.close();
  assert(!moved.iterationFailed());
  directory = Storage.open("/empty");
  assert(!directory.openNextFile());
  assert(!directory.iterationFailed());
  failNextRead = true;
  assert(!directory.seekSet(1));
  directory.close();
  assert(Storage.mkdir("/broken"));
  const std::string broken = std::string(argv[1]) + "/broken/missing";
  assert(symlink("does-not-exist", broken.c_str()) == 0);
  directory = Storage.open("/broken");
  assert(!directory.openNextFile());
  assert(directory.iterationFailed());
  directory.close();
  assert(Storage.mkdir("/child-open"));
  assert(Storage.mkdir("/child-open/child"));
  directory = Storage.open("/child-open");
  assert(directory);
  failNextOpenDirectory = true;
  assert(!directory.openNextFile());
  assert(directory.iterationFailed());
  directory.close();
  assert(Storage.mkdir("/valid"));
  std::ofstream(std::string(argv[1]) + "/valid/file") << "contents";
  directory = Storage.open("/valid");
  auto child = directory.openNextFile();
  assert(child && !directory.iterationFailed());
  child.close();
  assert(!directory.openNextFile() && !directory.iterationFailed());
  directory.close();
  assert(Storage.mkdir("/hidden"));
  std::ofstream(std::string(argv[1]) + "/hidden/.fixture") << "hidden";
  directory = Storage.open("/hidden");
  child = directory.openNextFile();
  assert(child && !directory.iterationFailed());
  char name[32]{};
  child.getName(name, sizeof(name));
  assert(std::string(name) == ".fixture");
  child.close();
  assert(!directory.openNextFile() && !directory.iterationFailed());
  directory.close();
}
