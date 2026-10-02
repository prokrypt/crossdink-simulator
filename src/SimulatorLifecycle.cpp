#include "SimulatorLifecycle.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace {

constexpr const char *kWakeReasonEnv = "CROSSPOINT_SIM_WAKE_REASON";
constexpr const char *kInputScriptEnv = "CROSSPOINT_SIM_INPUT_SCRIPT";
constexpr const char *kInputScriptAfterWakeEnv =
    "CROSSPOINT_SIM_INPUT_SCRIPT_AFTER_WAKE";
constexpr const char *kScreenshotsEnv = "CROSSPOINT_SIM_SCREENSHOTS";
constexpr const char *kScreenshotsAfterWakeEnv =
    "CROSSPOINT_SIM_SCREENSHOTS_AFTER_WAKE";
constexpr const char *kInputScriptAfterRebootEnv =
    "CROSSPOINT_SIM_INPUT_SCRIPT_AFTER_REBOOT";
constexpr const char *kScreenshotsAfterRebootEnv =
    "CROSSPOINT_SIM_SCREENSHOTS_AFTER_REBOOT";
constexpr const char *kSilentRebootMagicEnv =
    "CROSSPOINT_SIM_SILENT_REBOOT_MAGIC";
constexpr const char *kSilentRebootTargetEnv =
    "CROSSPOINT_SIM_SILENT_REBOOT_TARGET";
constexpr const char *kSilentRebootPayloadEnv =
    "CROSSPOINT_SIM_SILENT_REBOOT_PAYLOAD";
char **gArgv = nullptr;

void promoteAfterWakeValue(const char *target, const char *afterWake) {
  const char *value = std::getenv(afterWake);
  if (value) {
    setenv(target, value, 1);
  } else {
    unsetenv(target);
  }
  unsetenv(afterWake);
}

uint32_t readToken(const char *name) {
  const char* value = std::getenv(name);
  return value ? static_cast<uint32_t>(std::strtoul(value, nullptr, 10)) : 0;
}

void clearSilentRebootToken() {
  unsetenv(kSilentRebootMagicEnv);
  unsetenv(kSilentRebootTargetEnv);
  unsetenv(kSilentRebootPayloadEnv);
}

[[noreturn]] void reexec() {
  if (!gArgv || !gArgv[0]) {
    std::fputs("SimulatorLifecycle: missing argv for reboot\n", stderr);
    _exit(1);
  }

  execvp(gArgv[0], gArgv);

  std::perror("execvp");
  _exit(1);
}

} // namespace

namespace SimulatorLifecycle {

void initProcessArgs(char **argv) { gArgv = argv; }

WakeReason consumeWakeReason() {
  const char *value = std::getenv(kWakeReasonEnv);
  if (!value) {
    return WakeReason::None;
  }

  unsetenv(kWakeReasonEnv);
  if (std::strcmp(value, "power") == 0) {
    return WakeReason::PowerButton;
  }
  return WakeReason::None;
}

void setSilentRebootToken(const uint32_t magic, const uint32_t target, const uint32_t payload) {
  char value[11];
  std::snprintf(value, sizeof(value), "%u", magic);
  setenv(kSilentRebootMagicEnv, value, 1);
  std::snprintf(value, sizeof(value), "%u", target);
  setenv(kSilentRebootTargetEnv, value, 1);
  std::snprintf(value, sizeof(value), "%u", payload);
  setenv(kSilentRebootPayloadEnv, value, 1);
}

void restoreSilentRebootToken(uint32_t& magic, uint32_t& target, uint32_t& payload) {
  magic = readToken(kSilentRebootMagicEnv);
  target = readToken(kSilentRebootTargetEnv);
  payload = readToken(kSilentRebootPayloadEnv);
  clearSilentRebootToken();
}

[[noreturn]] void reboot() {
  // ESP.restart() relaunches the process with the same environment. Without
  // this, a scripted flow that triggers a firmware reboot (for example a
  // network-boot handoff) would replay its script from the start forever.
  // Tests that need to keep driving the relaunched instance provide the
  // optional *_AFTER_REBOOT schedules.
  promoteAfterWakeValue(kInputScriptEnv, kInputScriptAfterRebootEnv);
  promoteAfterWakeValue(kScreenshotsEnv, kScreenshotsAfterRebootEnv);
  reexec();
}

[[noreturn]] void rebootAsPowerWake() {
  clearSilentRebootToken();
  setenv(kWakeReasonEnv, "power", 1);
  // A deep-sleep wake is a fresh process. Do not replay the pre-sleep script,
  // which would otherwise put every relaunched process back to sleep forever.
  // Tests can provide an explicit post-wake schedule when they need to capture
  // or terminate the relaunched instance.
  promoteAfterWakeValue(kInputScriptEnv, kInputScriptAfterWakeEnv);
  promoteAfterWakeValue(kScreenshotsEnv, kScreenshotsAfterWakeEnv);
  reexec();
}

} // namespace SimulatorLifecycle
