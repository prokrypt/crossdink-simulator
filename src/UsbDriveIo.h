#pragma once

#include <cstdint>

// Host I/O totals while USB Drive is up. Mirrors the firmware's
// lib/hal/UsbDriveIo.h; the simulator never mounts a USB Drive, so these stay
// zero.
struct UsbDriveIo {
  uint32_t firstIoMs = 0; // millis() of the first host read/write, 0 = none
  uint32_t lastIoMs = 0;  // millis() of the last host read/write, 0 = none
  uint32_t ops = 0;       // host read/write calls
  uint32_t readBytes = 0;
  uint32_t writeBytes = 0;
};
