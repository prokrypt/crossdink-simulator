#include <Logging.h>

#include <HalStorage.h>

#include "network/FirmwareFlasher.h"
#include "network/OtaBootSwitch.h"

namespace firmware_flash {
Result flashFromSdPath(const char *, ProgressCb onProgress, void *ctx) {
  LOG_DBG("FLASH",
          "[SIM] Firmware flashing is not supported in the native simulator");
  if (onProgress)
    onProgress(1, 1, ctx);
  return Result::WRITE_FAIL;
}

Result validateImageFile(const char *, size_t) {
  LOG_DBG(
      "FLASH",
      "[SIM] Firmware image validation is disabled in the native simulator");
  return Result::WRITE_FAIL;
}

// CrossDink's split validate/flash API and header probes. Like the rest of
// this file, nothing is ever written: every image reads as unusable.
Result validateOpenImageFile(HalFile &, size_t) { return Result::WRITE_FAIL; }
Result flashValidatedFile(HalFile &, ProgressCb onProgress, void *ctx) {
  if (onProgress)
    onProgress(1, 1, ctx);
  return Result::WRITE_FAIL;
}
Result checkImageHeader(HalFile &, size_t) { return Result::WRITE_FAIL; }
Result checkImageHeaderFile(const char *, size_t) {
  return Result::WRITE_FAIL;
}
Result streamBegin(size_t) { return Result::WRITE_FAIL; }
Result streamWrite(const uint8_t *, size_t) { return Result::WRITE_FAIL; }
Result streamFinish() { return Result::WRITE_FAIL; }
void streamAbort() {}
bool streamActive() { return false; }
uint16_t runningPartitionChipId() { return 0; }

const char *resultName(Result r) {
  switch (r) {
  case Result::OK:
    return "OK";
  case Result::OPEN_FAIL:
    return "OPEN_FAIL";
  case Result::TOO_SMALL:
    return "TOO_SMALL";
  case Result::TOO_LARGE:
    return "TOO_LARGE";
  case Result::BAD_MAGIC:
    return "BAD_MAGIC";
  case Result::BAD_SEGMENTS:
    return "BAD_SEGMENTS";
  case Result::BAD_CHECKSUM:
    return "BAD_CHECKSUM";
  case Result::BAD_SHA:
    return "BAD_SHA";
  case Result::BAD_CHIP:
    return "BAD_CHIP";
  case Result::WRONG_BOARD:
    return "WRONG_BOARD";
  case Result::BAD_SIZE:
    return "BAD_SIZE";
  case Result::NO_PARTITION:
    return "NO_PARTITION";
  case Result::OOM:
    return "OOM";
  case Result::READ_FAIL:
    return "READ_FAIL";
  case Result::ERASE_FAIL:
    return "ERASE_FAIL";
  case Result::WRITE_FAIL:
    return "UNSUPPORTED_IN_SIMULATOR";
  case Result::OTADATA_FAIL:
    return "OTADATA_FAIL";
  default:
    return "UNKNOWN";
  }
}
} // namespace firmware_flash

namespace ota_boot {
uint32_t computeSeqCrc(uint32_t) { return 0; }
bool switchTo(const esp_partition_t *) {
  LOG_DBG("FLASH", "[SIM] Boot partition switching is not supported in the "
                   "native simulator");
  return false;
}
} // namespace ota_boot
