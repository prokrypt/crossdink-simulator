#include "HalPowerManager.h"

#include "HalGPIO.h"

HalPowerManager powerManager;

void HalPowerManager::begin() {}
void HalPowerManager::startDeepSleep(HalGPIO &gpio) const { gpio.startDeepSleep(); }
void HalPowerManager::setPowerSaving(bool enable) {}
uint16_t HalPowerManager::getBatteryPercentage() const { return 100; }
uint16_t HalPowerManager::getBatteryPercent256() const { return 100 * 256; }

void HalPowerManager::beginDisplayBusyWait() {}
void HalPowerManager::endDisplayBusyWait() {}
void HalPowerManager::beginDisplayRefreshHold() {}
void HalPowerManager::endDisplayRefreshHold() {}
void HalPowerManager::setUsbDriveActive(bool) {}
void HalPowerManager::beginBackgroundWork() {}
void HalPowerManager::endBackgroundWork() {}

#if CROSSDINK_BATTERY_DIAG_LOG
bool HalPowerManager::getBatteryDiagnostics(BatteryDiagnostics &) const {
  return false;
}
#endif

HalPowerManager::Lock::Lock() {}
HalPowerManager::Lock::~Lock() {}
