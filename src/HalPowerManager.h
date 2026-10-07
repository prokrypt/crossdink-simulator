#pragma once

#include <Arduino.h>
#include <InputManager.h>
#include <Logging.h>
#include <freertos/semphr.h>

#include <cassert>
#include <cstdint>

#include "HalGPIO.h"

#ifndef CROSSDINK_BATTERY_DIAG_LOG
#define CROSSDINK_BATTERY_DIAG_LOG 0
#endif

class HalPowerManager;
extern HalPowerManager powerManager; // Singleton

class HalPowerManager {
  int normalFreq = 0; // MHz
  bool isLowPower = false;
  bool refreshLightSleep = false;
  bool radioIdleSleepAllowed = false;

  enum LockMode { None, NormalSpeed };
  LockMode currentLockMode = None;
  SemaphoreHandle_t modeMutex = nullptr; // Protect access to currentLockMode

public:
  static constexpr int LOW_POWER_FREQ = 10;                   // MHz
  static constexpr int DFS_MIN_FREQ = 80;                     // MHz
  static constexpr unsigned long IDLE_POWER_SAVING_MS = 3000; // ms
  static constexpr unsigned long BATTERY_POLL_MS = 10000;     // ms

  void begin();

  // Control CPU frequency for power saving
  void setPowerSaving(bool enabled);

  // Light-sleep and clock holds. The host has no light sleep or DFS, so these
  // only keep the firmware's bookkeeping calls compiling.
  void beginDisplayBusyWait();
  void endDisplayBusyWait();
  void setRefreshLightSleep(bool allowed) { refreshLightSleep = allowed; }
  bool refreshLightSleepAllowed() const { return refreshLightSleep; }
  void beginDisplayRefreshHold();
  void endDisplayRefreshHold();
  void setUsbDriveActive(bool active);
  void setRadioIdleSleepAllowed(bool allowed) {
    radioIdleSleepAllowed = allowed;
  }
  void beginBackgroundWork();
  void endBackgroundWork();

  // Setup wake up GPIO and enter deep sleep
  // Should be called inside main loop() to handle the currentLockMode
  void startDeepSleep(HalGPIO &gpio) const;
  // The sleep-entry step in progress, for the firmware's stuck-sleep log.
  static inline const char *volatile sleepStep = "";
  // Charger-change wake request; the simulator has no charger to watch.
  bool wakeOnChargeChange = false;

  // Get battery percentage (range 0-100)
  uint16_t getBatteryPercentage() const;
  // Same in 1/256 %.
  uint16_t getBatteryPercent256() const;

#if CROSSDINK_BATTERY_DIAG_LOG
  struct BatteryDiagnostics {
    uint16_t soc = 0;
    uint16_t millivolts = 0;
    bool charging = false;
    bool socKnown = false;
    bool millivoltsKnown = false;
    bool chargingKnown = false;
  };
  // No battery telemetry on the host.
  bool getBatteryDiagnostics(BatteryDiagnostics &out) const;
#endif

  // RAII helper class to manage power saving locks
  // Usage: create an instance of Lock in a scope to disable power saving, for
  // example when running a task that needs full performance. When the Lock
  // instance is destroyed (goes out of scope), power saving will be re-enabled.
  class Lock {
    friend class HalPowerManager;
    bool valid = false;

  public:
    explicit Lock();
    ~Lock();

    // Non-copyable and non-movable
    Lock(const Lock &) = delete;
    Lock &operator=(const Lock &) = delete;
    Lock(Lock &&) = delete;
    Lock &operator=(Lock &&) = delete;
  };
};
