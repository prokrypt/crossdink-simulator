#pragma once

#include <Arduino.h>
#include <BatteryMonitor.h>
#include <InputManager.h>

// Display SPI pins (custom pins for XteinkX4, not hardware SPI defaults)
#ifndef EPD_SCLK
#define EPD_SCLK 8 // SPI Clock
#endif
#ifndef EPD_MOSI
#define EPD_MOSI 10 // SPI MOSI (Master Out Slave In)
#endif
#ifndef EPD_CS
#define EPD_CS 21 // Chip Select
#endif
#ifndef EPD_DC
#define EPD_DC 4 // Data/Command
#endif
#ifndef EPD_RST
#define EPD_RST 5 // Reset
#endif
#ifndef EPD_BUSY
#define EPD_BUSY 6 // Busy
#endif

#define SPI_MISO                                                               \
  7 // SPI MISO, shared between SD card and display (Master In Slave Out)

#define BAT_GPIO0 0 // Battery voltage

#define UART0_RXD 20 // Used for USB connection detection

class HalGPIO {
#if CROSSPOINT_EMULATED == 0
  InputManager inputMgr;
#endif

public:
  struct TouchContact {
    uint8_t id = 0;
    float nx = 0.0f;
    float ny = 0.0f;
  };

  struct TouchSnapshot {
    uint8_t count = 0;
    uint8_t reportedCount = 0;
    TouchContact contacts[2];
  };

  struct CompletedMultiTouchSwipe {
    uint8_t contactCount = 0;
    float nxStart = 0.0f;
    float nyStart = 0.0f;
    float nxEnd = 0.0f;
    float nyEnd = 0.0f;
    unsigned long durationMs = 0;
  };

  struct CompletedMultiTouchRotation {
    float degrees = 0.0f;
    float nxCenter = 0.0f;
    float nyCenter = 0.0f;
    unsigned long durationMs = 0;
  };

  enum class DeviceType : uint8_t { X4, X3 };

private:
  DeviceType _deviceType = DeviceType::X4;

public:
  HalGPIO() = default;

  // Inline device type helpers for cleaner downstream checks
  inline bool deviceIsX3() const { return _deviceType == DeviceType::X3; }
  inline bool deviceIsX4() const { return _deviceType == DeviceType::X4; }
  bool isXteinkDevice() const;
  bool hasEdgeSideButtons() const;
  // False on boards whose only physical nav keys are Up/Down (X4 Pro, Sticky).
  bool hasLeftRightButtons() const;

  // Start button GPIO and setup SPI for screen and SD card
  void begin();

  // Clears the per-frame press/release edge latches. Must be called exactly
  // once per frame (before the firmware's loop()), NOT on every update().
  void beginFrame();

  // Button input methods
  void update();

  // The firmware hands sampling to an input task on device. HalGPIO::update()
  // owns the SDL event pump here, which must stay on the main thread, so
  // latched input never starts and update() keeps sampling directly.
  bool startLatchedInput();
  void stopLatchedInput();
  struct SampleResult {
    bool events;
    bool active;
  };
  SampleResult sampleInput();
  bool isPressed(uint8_t buttonIndex) const;
  bool wasPressed(uint8_t buttonIndex) const;
  bool wasAnyPressed() const;
  bool wasReleased(uint8_t buttonIndex) const;
  bool wasAnyReleased() const;
  unsigned long getHeldTime() const;
  unsigned long getPowerButtonHeldTime() const;
  bool hasTouch() const;
  // SDL currently injects one pointer, but touch device profiles should still
  // expose their hardware capability so firmware settings can be inspected.
  bool supportsMultiTouch() const;
  constexpr TouchSnapshot getTouchSnapshot() const { return {}; }
  constexpr bool wasCompletedMultiTouchSwipe(CompletedMultiTouchSwipe&) const { return false; }
  constexpr bool wasCompletedMultiTouchRotation(CompletedMultiTouchRotation&) const { return false; }
  bool hasHomeKey() const;
  bool wasHomeKeyPressed() const;
  bool wasHomeKeyTapped() const;
  bool wasHomeKeyLongPressed() const;
  bool wasTouchTap(float &nx, float &ny) const;
  bool wasTouchDown(float &nx, float &ny) const;
  bool wasTouchReleased() const;
  bool isTouchTapCandidate(float &nx, float &ny, unsigned long &heldMs) const;
  bool wasTouchLongPress(float &nx, float &ny) const;
  void suppressTouchContact();
  bool isTouchHeldAt(float &nx, float &ny) const;
  unsigned long lastTouchHeldMs() const;
  bool wasSwipe(float &nxStart, float &nyStart, float &nxEnd,
                float &nyEnd) const;
  bool wasTouchActivity() const;
  // No touch controller to power down on the host.
  bool setTouchSleep(bool asleep);
  bool isTouchAsleep() const;
  void setSharedConfirmPowerShortPressEmitsPower(bool enabled);
  bool consumeSimulatorSleepRequest();

  // Setup wake up GPIO and enter deep sleep
  void startDeepSleep();

  // Simulated power-button wakes are always accepted so host boot can continue.
  bool verifyPowerButtonWakeup(bool shortPressWakes, uint16_t longHoldMs);

  // Check if USB is connected
  bool isUsbConnected() const;
  bool isUsbConnectedCached() const;
  // Host launches are cold boots with "USB" attached; never treat them as a
  // power-button boot that should go straight back to sleep.
  bool coldBootImpliesPowerButton() const;

  // Returns true once per edge (plug or unplug) since the last update()
  bool wasUsbStateChanged() const;

  enum class WakeupReason { PowerButton, AfterFlash, AfterUSBPower, Other };

  WakeupReason getWakeupReason() const;

  // Button indices
  static constexpr uint8_t BTN_BACK = 0;
  static constexpr uint8_t BTN_CONFIRM = 1;
  static constexpr uint8_t BTN_LEFT = 2;
  static constexpr uint8_t BTN_RIGHT = 3;
  static constexpr uint8_t BTN_UP = 4;
  static constexpr uint8_t BTN_DOWN = 5;
  static constexpr uint8_t BTN_POWER = 6;
};

extern HalGPIO gpio; // Singleton
