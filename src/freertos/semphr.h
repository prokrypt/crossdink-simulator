#pragma once
#include <condition_variable>
#include <mutex>

#include "FreeRTOS.h"
#include "task.h"

// FreeRTOS semaphores on the host. Mutexes (plain and recursive) are backed
// by a recursive timed mutex so the render lock nests the way the firmware
// expects; binary and counting semaphores are a counter guarded by a condvar,
// because they are routinely given by a different task than the one taking
// them, which a mutex must not do.
struct SimMutex {
  enum class Kind : uint8_t { Mutex, Counting };
  Kind kind = Kind::Mutex;

  // Kind::Mutex
  std::recursive_timed_mutex mtx;
  // Track "holder" for xSemaphoreGetMutexHolder compatibility (not
  // thread-safe, good enough for simulator)
  TaskHandle_t holder = nullptr;
  uint16_t holdCount = 0;

  // Kind::Counting
  std::mutex countMtx;
  std::condition_variable countCv;
  uint32_t count = 0;
  uint32_t maxCount = 1;
};
typedef SimMutex *SemaphoreHandle_t;
struct StaticSemaphore_t {};

inline SemaphoreHandle_t xSemaphoreCreateMutex() { return new SimMutex(); }
inline SemaphoreHandle_t xSemaphoreCreateRecursiveMutex() {
  return new SimMutex();
}
inline SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *) {
  return new SimMutex();
}
inline SemaphoreHandle_t
xSemaphoreCreateRecursiveMutexStatic(StaticSemaphore_t *) {
  return new SimMutex();
}

inline SemaphoreHandle_t xSemaphoreCreateCounting(UBaseType_t maxCount,
                                                  UBaseType_t initialCount) {
  auto *sem = new SimMutex();
  sem->kind = SimMutex::Kind::Counting;
  sem->maxCount = maxCount;
  sem->count = initialCount;
  return sem;
}
// Binary semaphores start empty, as in FreeRTOS.
inline SemaphoreHandle_t xSemaphoreCreateBinary() {
  return xSemaphoreCreateCounting(1, 0);
}
inline SemaphoreHandle_t xSemaphoreCreateBinaryStatic(StaticSemaphore_t *) {
  return xSemaphoreCreateBinary();
}

inline void vSemaphoreDelete(SemaphoreHandle_t sem) { delete sem; }

inline BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, TickType_t ticksToWait) {
  if (!sem)
    return pdTRUE;
  if (sem->kind == SimMutex::Kind::Counting) {
    std::unique_lock<std::mutex> lk(sem->countMtx);
    const auto ready = [sem] { return sem->count > 0; };
    if (!ready()) {
      if (ticksToWait == 0)
        return pdFALSE;
      SimBlockedScope blocked(tl_currentTaskHandle);
      if (ticksToWait == portMAX_DELAY) {
        sem->countCv.wait(lk, ready);
      } else if (!sem->countCv.wait_until(lk, simDeadline(ticksToWait),
                                          ready)) {
        return pdFALSE;
      }
    }
    sem->count--;
    return pdTRUE;
  }

  bool locked;
  if (ticksToWait == portMAX_DELAY) {
    SimBlockedScope blocked(tl_currentTaskHandle);
    sem->mtx.lock();
    locked = true;
  } else if (ticksToWait == 0) {
    locked = sem->mtx.try_lock();
  } else {
    SimBlockedScope blocked(tl_currentTaskHandle);
    locked = sem->mtx.try_lock_for(std::chrono::milliseconds(ticksToWait));
  }
  if (!locked)
    return pdFALSE;
  sem->holder = xTaskGetCurrentTaskHandle();
  sem->holdCount++;
  return pdTRUE;
}

inline BaseType_t xSemaphoreGive(SemaphoreHandle_t sem) {
  if (!sem)
    return pdTRUE;
  if (sem->kind == SimMutex::Kind::Counting) {
    {
      std::lock_guard<std::mutex> lk(sem->countMtx);
      if (sem->count >= sem->maxCount)
        return pdFALSE;
      sem->count++;
    }
    sem->countCv.notify_one();
    return pdTRUE;
  }
  // Only the holder may give a mutex; FreeRTOS refuses anyone else.
  if (sem->holdCount == 0 || sem->holder != xTaskGetCurrentTaskHandle())
    return pdFALSE;
  sem->holdCount--;
  if (sem->holdCount == 0) {
    sem->holder = nullptr;
  }
  sem->mtx.unlock();
  return pdTRUE;
}

inline BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t sem,
                                          TickType_t ticksToWait) {
  return xSemaphoreTake(sem, ticksToWait);
}
inline BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t sem) {
  return xSemaphoreGive(sem);
}

inline BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t sem,
                                        BaseType_t *higherPriorityTaskWoken) {
  if (higherPriorityTaskWoken)
    *higherPriorityTaskWoken = pdFALSE;
  return xSemaphoreGive(sem);
}
inline BaseType_t xSemaphoreTakeFromISR(SemaphoreHandle_t sem,
                                        BaseType_t *higherPriorityTaskWoken) {
  if (higherPriorityTaskWoken)
    *higherPriorityTaskWoken = pdFALSE;
  return xSemaphoreTake(sem, 0);
}

inline UBaseType_t uxSemaphoreGetCount(SemaphoreHandle_t sem) {
  if (!sem)
    return 0;
  if (sem->kind == SimMutex::Kind::Counting) {
    std::lock_guard<std::mutex> lk(sem->countMtx);
    return sem->count;
  }
  return sem->holdCount == 0 ? 1 : 0;
}

inline TaskHandle_t xSemaphoreGetMutexHolder(SemaphoreHandle_t sem) {
  return sem ? sem->holder : nullptr;
}

// xQueuePeek on a semaphore: returns pdTRUE if it is available (a mutex not
// taken, or a counting semaphore with a count).
inline int xQueuePeek(SemaphoreHandle_t sem, void *, uint32_t) {
  if (!sem)
    return pdTRUE;
  if (sem->kind == SimMutex::Kind::Counting)
    return uxSemaphoreGetCount(sem) > 0 ? pdTRUE : pdFALSE;
  bool locked = sem->mtx.try_lock();
  if (locked) {
    sem->mtx.unlock();
    return pdTRUE;
  }
  return pdFALSE;
}
