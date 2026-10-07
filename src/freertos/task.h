#pragma once
#include <cstdint>
#include <cstring>

#include "FreeRTOS.h"

// Thread-local pointer so ulTaskNotifyTake can find the current task's handle.
inline thread_local SimTaskHandle *tl_currentTaskHandle = nullptr;

inline SimTaskHandle *simMainTaskHandle() {
  static SimTaskHandle mainTask;
  static std::once_flag initFlag;
  std::call_once(initFlag, [] {
    mainTask.name = "main";
    mainTask.id = std::this_thread::get_id();
  });
  return &mainTask;
}

inline TaskHandle_t xTaskGetCurrentTaskHandle() {
  return tl_currentTaskHandle ? tl_currentTaskHandle : simMainTaskHandle();
}

// Create a real OS thread. The FreeRTOS task function signature is
// void(*)(void*).
inline BaseType_t xTaskCreate(void (*fn)(void *), const char *name,
                              uint32_t stackDepth, void *param,
                              BaseType_t /*priority*/, TaskHandle_t *handle) {
  auto *h = new SimTaskHandle();
  h->name = name ? name : "sim-task";
  h->stackUsage->budget = stackDepth;
  simStackCheckTaskBudget(h->name, stackDepth);
  h->thread =
      std::thread([fn, param, h, usage = h->stackUsage, taskName = h->name]() {
        tl_currentTaskHandle = h;
        h->id = std::this_thread::get_id();
        volatile char stackAnchor;
        simStackBegin(usage.get(), taskName,
                      reinterpret_cast<uintptr_t>(&stackAnchor));
        fn(param);
        simStackEnd();
      });
  if (handle)
    *handle = h;
  return 1; // pdPASS
}

// The host simulator has no FreeRTOS stack allocator. Keep the static-task
// API compatible while delegating thread creation to the normal simulator
// task path; the provided storage is only meaningful on firmware.
inline TaskHandle_t xTaskCreateStatic(void (*fn)(void *), const char *name,
                                      uint32_t stackDepth, void *param,
                                      BaseType_t priority,
                                      StackType_t * /*stack*/,
                                      StaticTask_t * /*taskStorage*/) {
  TaskHandle_t handle = nullptr;
  return xTaskCreate(fn, name, stackDepth, param, priority, &handle) == pdPASS
             ? handle
             : nullptr;
}

// Core pinning has no meaning on the host; delegate to xTaskCreate and
// ignore the core ID.
inline BaseType_t xTaskCreatePinnedToCore(void (*fn)(void *), const char *name,
                                          uint32_t stackDepth, void *param,
                                          BaseType_t priority,
                                          TaskHandle_t *handle,
                                          BaseType_t /*coreId*/) {
  return xTaskCreate(fn, name, stackDepth, param, priority, handle);
}

// Wait for a non-zero notification value, as FreeRTOS does: returns the value
// before clearing (clearOnExit) or decrementing it, or 0 on timeout.
inline uint32_t ulTaskNotifyTake(BaseType_t clearOnExit,
                                 TickType_t ticksToWait) {
  auto *h = xTaskGetCurrentTaskHandle();
  std::unique_lock<std::mutex> lk(h->mtx);
  const auto ready = [h] { return h->notifyValue != 0; };
  if (!ready()) {
    if (ticksToWait == 0)
      return 0;
    SimBlockedScope blocked(h);
    if (ticksToWait == portMAX_DELAY) {
      h->cv.wait(lk, ready);
    } else if (!h->cv.wait_until(lk, simDeadline(ticksToWait), ready)) {
      return 0;
    }
  }
  const uint32_t value = h->notifyValue;
  h->notifyValue = clearOnExit ? 0 : value - 1;
  return value;
}

// Update a task's notification value and wake it if it is waiting.
inline BaseType_t xTaskNotify(TaskHandle_t handle, uint32_t value,
                              int action) {
  if (!handle)
    return pdFAIL;
  {
    std::lock_guard<std::mutex> lk(handle->mtx);
    switch (action) {
    case eSetBits:
      handle->notifyValue |= value;
      break;
    case eIncrement:
      handle->notifyValue++;
      break;
    case eSetValueWithOverwrite:
      handle->notifyValue = value;
      break;
    case eSetValueWithoutOverwrite:
      if (handle->notifyValue != 0)
        return pdFAIL;
      handle->notifyValue = value;
      break;
    default:
      break;
    }
  }
  handle->cv.notify_all();
  return pdPASS;
}

inline BaseType_t xTaskNotifyGive(TaskHandle_t handle) {
  return xTaskNotify(handle, 0, eIncrement);
}

inline BaseType_t xTaskNotifyFromISR(TaskHandle_t handle, uint32_t value,
                                     int action, BaseType_t *woken) {
  if (woken)
    *woken = pdFALSE;
  return xTaskNotify(handle, value, action);
}

inline void vTaskNotifyGiveFromISR(TaskHandle_t handle, BaseType_t *woken) {
  if (woken)
    *woken = pdFALSE;
  xTaskNotifyGive(handle);
}

// ESP-IDF's *WithCaps variants place the task stack in a chosen heap (PSRAM
// on device). Host threads get their own stacks, so the caps are ignored.
inline BaseType_t xTaskCreatePinnedToCoreWithCaps(
    void (*fn)(void *), const char *name, uint32_t stackDepth, void *param,
    UBaseType_t priority, TaskHandle_t *handle, BaseType_t coreId,
    uint32_t /*memoryCaps*/) {
  return xTaskCreatePinnedToCore(fn, name, stackDepth, param,
                                 static_cast<BaseType_t>(priority), handle,
                                 coreId);
}
inline BaseType_t xTaskCreateWithCaps(void (*fn)(void *), const char *name,
                                      uint32_t stackDepth, void *param,
                                      UBaseType_t priority,
                                      TaskHandle_t *handle,
                                      uint32_t /*memoryCaps*/) {
  return xTaskCreate(fn, name, stackDepth, param,
                     static_cast<BaseType_t>(priority), handle);
}

inline const char *pcTaskGetName(TaskHandle_t h) {
  if (!h)
    h = xTaskGetCurrentTaskHandle();
  return h ? h->name : "main";
}
inline void vTaskDelete(TaskHandle_t h) {
  if (h) {
    if (h->thread.joinable())
      h->thread.detach();
    delete h;
  }
}
// Zero means unavailable when stack instrumentation is disabled or for main.
// Instrumented values are sampled host headroom, not device measurements.
inline unsigned int uxTaskGetStackHighWaterMark(TaskHandle_t h) {
  if (!h || h == tl_currentTaskHandle)
    return simStackCurrentMinimumFree();
  return simStackMinimumFree(h->stackUsage.get());
}
// Firmware parks a *WithCaps task (vTaskSuspend) and its owner deletes it
// once the task has signalled completion, which can be just before it parks.
// A host thread cannot be killed, so the parked thread stays blocked and its
// handle is kept alive for it rather than freed under it.
inline void vTaskDeleteWithCaps(TaskHandle_t h) {
  if (h && h->thread.joinable())
    h->thread.detach();
}

inline void vTaskList(char *) {}
inline void vTaskDelay(TickType_t ticks) {
  if (ticks == 0) {
    std::this_thread::yield();
    return;
  }
  SimBlockedScope blocked(tl_currentTaskHandle);
  std::this_thread::sleep_for(std::chrono::milliseconds(ticks));
}

inline TickType_t xTaskGetTickCount() {
  using namespace std::chrono;
  static const auto start = steady_clock::now();
  return static_cast<TickType_t>(
      duration_cast<milliseconds>(steady_clock::now() - start).count());
}

inline eTaskState eTaskGetState(TaskHandle_t h) {
  if (!h)
    return eInvalid;
  return static_cast<eTaskState>(h->state.load());
}

// Suspending the calling task parks it for good: firmware workers do this
// after their last touch of shared state, then wait to be deleted. Nothing in
// the firmware resumes a suspended task, so other handles are left running.
inline void vTaskSuspend(TaskHandle_t h) {
  SimTaskHandle *self = xTaskGetCurrentTaskHandle();
  if (h && h != self)
    return;
  self->state.store(eSuspended);
  static std::mutex parkMtx;
  static std::condition_variable parkCv;
  std::unique_lock<std::mutex> lk(parkMtx);
  parkCv.wait(lk, [] { return false; });
}
inline void vTaskResume(TaskHandle_t) {}
