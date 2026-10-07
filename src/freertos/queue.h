#pragma once
#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <vector>

#include "FreeRTOS.h"
#include "task.h"

// FreeRTOS queues on the host: fixed-size items copied in and out, bounded
// length, and blocking send/receive with the usual tick timeouts.
struct SimQueue {
  size_t itemSize = 0;
  UBaseType_t length = 0;
  std::deque<std::vector<uint8_t>> items;
  std::mutex mtx;
  std::condition_variable notEmpty;
  std::condition_variable notFull;
};
typedef SimQueue *QueueHandle_t;

inline QueueHandle_t xQueueCreate(UBaseType_t length, UBaseType_t itemSize) {
  if (length == 0)
    return nullptr;
  auto *q = new SimQueue();
  q->length = length;
  q->itemSize = itemSize;
  return q;
}

inline void vQueueDelete(QueueHandle_t q) { delete q; }

namespace sim_queue {
template <typename Pred>
inline bool waitFor(std::condition_variable &cv,
                    std::unique_lock<std::mutex> &lk, TickType_t ticks,
                    Pred pred) {
  if (pred())
    return true;
  if (ticks == 0)
    return false;
  SimBlockedScope blocked(tl_currentTaskHandle);
  if (ticks == portMAX_DELAY) {
    cv.wait(lk, pred);
    return true;
  }
  return cv.wait_until(lk, simDeadline(ticks), pred);
}

inline BaseType_t send(QueueHandle_t q, const void *item, TickType_t ticks,
                       bool front, bool overwrite) {
  if (!q)
    return pdFAIL;
  std::unique_lock<std::mutex> lk(q->mtx);
  if (overwrite && q->items.size() >= q->length) {
    q->items.pop_back();
  } else if (!waitFor(q->notFull, lk, ticks,
                      [q] { return q->items.size() < q->length; })) {
    return errQUEUE_FULL;
  }
  std::vector<uint8_t> copy(q->itemSize);
  if (q->itemSize && item)
    memcpy(copy.data(), item, q->itemSize);
  if (front)
    q->items.push_front(std::move(copy));
  else
    q->items.push_back(std::move(copy));
  lk.unlock();
  q->notEmpty.notify_one();
  return pdPASS;
}
} // namespace sim_queue

inline BaseType_t xQueueSend(QueueHandle_t q, const void *item,
                             TickType_t ticks) {
  return sim_queue::send(q, item, ticks, false, false);
}
inline BaseType_t xQueueSendToBack(QueueHandle_t q, const void *item,
                                   TickType_t ticks) {
  return sim_queue::send(q, item, ticks, false, false);
}
inline BaseType_t xQueueSendToFront(QueueHandle_t q, const void *item,
                                    TickType_t ticks) {
  return sim_queue::send(q, item, ticks, true, false);
}
inline BaseType_t xQueueOverwrite(QueueHandle_t q, const void *item) {
  return sim_queue::send(q, item, 0, false, true);
}
inline BaseType_t xQueueSendFromISR(QueueHandle_t q, const void *item,
                                    BaseType_t *woken) {
  if (woken)
    *woken = pdFALSE;
  return sim_queue::send(q, item, 0, false, false);
}

inline BaseType_t xQueueReceive(QueueHandle_t q, void *out, TickType_t ticks) {
  if (!q)
    return pdFAIL;
  std::unique_lock<std::mutex> lk(q->mtx);
  if (!sim_queue::waitFor(q->notEmpty, lk, ticks,
                          [q] { return !q->items.empty(); }))
    return errQUEUE_EMPTY;
  if (q->itemSize && out)
    memcpy(out, q->items.front().data(), q->itemSize);
  q->items.pop_front();
  lk.unlock();
  q->notFull.notify_one();
  return pdPASS;
}

inline BaseType_t xQueuePeek(QueueHandle_t q, void *out, TickType_t ticks) {
  if (!q)
    return pdFAIL;
  std::unique_lock<std::mutex> lk(q->mtx);
  if (!sim_queue::waitFor(q->notEmpty, lk, ticks,
                          [q] { return !q->items.empty(); }))
    return errQUEUE_EMPTY;
  if (q->itemSize && out)
    memcpy(out, q->items.front().data(), q->itemSize);
  return pdPASS;
}

inline UBaseType_t uxQueueMessagesWaiting(QueueHandle_t q) {
  if (!q)
    return 0;
  std::lock_guard<std::mutex> lk(q->mtx);
  return static_cast<UBaseType_t>(q->items.size());
}

inline UBaseType_t uxQueueSpacesAvailable(QueueHandle_t q) {
  if (!q)
    return 0;
  std::lock_guard<std::mutex> lk(q->mtx);
  return q->length - static_cast<UBaseType_t>(q->items.size());
}

inline BaseType_t xQueueReset(QueueHandle_t q) {
  if (!q)
    return pdFAIL;
  {
    std::lock_guard<std::mutex> lk(q->mtx);
    q->items.clear();
  }
  q->notFull.notify_all();
  return pdPASS;
}
