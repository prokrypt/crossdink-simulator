#pragma once

#include <Arduino.h>

// Host model of the X4 Pro dual-channel frontlight. The framebuffer is
// unchanged; this class preserves the firmware-visible brightness, warmth, and
// on/off state so the quick panel and persisted settings can be exercised.
class HalFrontlight {
public:
  static HalFrontlight &getInstance();

  void begin(uint8_t brightness, uint8_t warmth, bool on);

  bool present() const;
  bool hasColorTemperature() const;

  void setBrightness(uint8_t percent);
  void setWarmth(uint8_t warmPercent);
  void setOn(bool on);
  // Light timeout dim and transfer throb overlay. Tracked for the firmware's
  // state checks; the host window has no light to drive. setBrightness() and
  // setOn() end both, as on device.
  void setIdleDim(uint8_t percent);
  uint8_t idleDimPercent() const { return idleDim; }
  static constexpr uint8_t NO_OVERLAY = 0xFF;
  void setOverlay(uint8_t percent);
  bool overlayActive() const { return overlay != NO_OVERLAY; }
  void prepareForDeepSleep() {}
  void releaseAfterWake() {}
  void flushLog() {}

  uint8_t brightness() const;
  uint8_t warmth() const;
  bool isOn() const;

private:
  uint8_t lastBrightness = 60;
  uint8_t lastWarmth = 50;
  bool lit = false;
  uint8_t idleDim = 100;
  uint8_t overlay = NO_OVERLAY;
};

#define Frontlight HalFrontlight::getInstance()
