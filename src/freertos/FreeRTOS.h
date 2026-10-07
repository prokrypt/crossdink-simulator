#pragma once
#include "../SimulatorStackCheck.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>

#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define pdFAIL 0
#define errQUEUE_EMPTY 0
#define errQUEUE_FULL 0
#define portMAX_DELAY 0xFFFFFFFF
#define portTICK_PERIOD_MS 1
#define configTICK_RATE_HZ 1000

using BaseType_t = int;
using UBaseType_t = unsigned int;
// One tick is one millisecond, so tick counts double as millisecond timeouts.
using TickType_t = uint32_t;
#define pdMS_TO_TICKS(ms) (static_cast<TickType_t>(ms))
#define pdTICKS_TO_MS(ticks) (static_cast<uint32_t>(ticks))

typedef void (*TaskFunction_t)(void *);

// Task notification actions, in FreeRTOS order.
enum eNotifyAction {
  eNoAction = 0,
  eSetBits,
  eIncrement,
  eSetValueWithOverwrite,
  eSetValueWithoutOverwrite,
};

// Task states, in FreeRTOS order (firmware indexes "RrBSDI" with them).
enum eTaskState { eRunning = 0, eReady, eBlocked, eSuspended, eDeleted, eInvalid };

// Every host thread reports core 0; tasks are not pinned on the host.
inline BaseType_t xPortGetCoreID() { return 0; }
#define portYIELD_FROM_ISR(...) ((void)0)
#define portYIELD() std::this_thread::yield()
#define taskYIELD() std::this_thread::yield()

// ESP-IDF's portMUX_TYPE is a spinlock used with taskENTER_CRITICAL /
// taskEXIT_CRITICAL to guard data shared between tasks (and, on multi-core
// targets, cores). The simulator has no real critical-section primitive, so
// back it with a real mutex to preserve the same mutual-exclusion semantics
// across host threads.
struct SimPortMux {
  std::recursive_mutex mtx;
};
typedef SimPortMux portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED                                           \
  {                                                                            \
  }

inline void taskENTER_CRITICAL(portMUX_TYPE *mux) { mux->mtx.lock(); }
inline void taskEXIT_CRITICAL(portMUX_TYPE *mux) { mux->mtx.unlock(); }
#define portENTER_CRITICAL(mux) taskENTER_CRITICAL(mux)
#define portEXIT_CRITICAL(mux) taskEXIT_CRITICAL(mux)

// TaskHandle wraps a real thread + a notification counter protected by a
// condvar.
struct SimTaskHandle {
  std::thread thread;
  // A detached host task can outlive its public handle. Keep its measurement
  // record alive through the thread capture as well as the handle.
  std::shared_ptr<SimStackUsage> stackUsage = std::make_shared<SimStackUsage>();
  std::mutex mtx;
  std::condition_variable cv;
  // The FreeRTOS notification value: counted by eIncrement/xTaskNotifyGive,
  // or a bit set by eSetBits. ulTaskNotifyTake returns it whole.
  uint32_t notifyValue = 0;
  std::thread::id id;
  const char *name = "sim-task";
  // eRunning while the thread executes, eBlocked while it waits in a
  // notification, semaphore or queue call, eSuspended once parked.
  std::atomic<int> state{eRunning};
};

// Marks the current task blocked for the duration of a wait.
struct SimBlockedScope {
  std::atomic<int> *state;
  explicit SimBlockedScope(SimTaskHandle *h)
      : state(h ? &h->state : nullptr) {
    if (state)
      state->store(eBlocked);
  }
  ~SimBlockedScope() {
    if (state)
      state->store(eRunning);
  }
  SimBlockedScope(const SimBlockedScope &) = delete;
  SimBlockedScope &operator=(const SimBlockedScope &) = delete;
};

// A relative FreeRTOS timeout as an absolute steady-clock deadline.
inline std::chrono::steady_clock::time_point simDeadline(uint32_t ticks) {
  return std::chrono::steady_clock::now() + std::chrono::milliseconds(ticks);
}
// Static task allocation is an ESP-IDF storage contract. The simulator uses
// std::thread instead, but provides these placeholders so firmware using that
// API builds against the same interface.
typedef uint32_t StackType_t;
struct StaticTask_t {};
typedef SimTaskHandle *TaskHandle_t;
