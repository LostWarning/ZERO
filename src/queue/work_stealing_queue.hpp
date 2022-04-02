#ifndef __QUEUE_WORK_STEALING_QUEUE_HPP__
#define __QUEUE_WORK_STEALING_QUEUE_HPP__

#include "circular_array.hpp"

#include <atomic>
#include <iostream>

template <typename T>
class work_stealing_queue {

  std::atomic<size_t> m_front;
  std::atomic<size_t> m_back;
  std::atomic<circular_array<T> *> m_data{nullptr};
  std::atomic<circular_array<T> *> m_old{nullptr};

public:
  explicit work_stealing_queue(size_t capacity) {
    m_front.store(1, std::memory_order_relaxed);
    m_back.store(1, std::memory_order_relaxed);
    m_data.store(new circular_array<T>(capacity), std::memory_order_relaxed);
  }

  ~work_stealing_queue() { delete m_data.load(std::memory_order_relaxed); }

  bool empty() const noexcept {
    size_t back  = m_back.load(std::memory_order_relaxed);
    size_t front = m_front.load(std::memory_order_relaxed);
    return back == front;
  }

  void enqueue(const T &item) {
    size_t back             = m_back.load(std::memory_order_relaxed);
    size_t front            = m_front.load(std::memory_order_acquire);
    circular_array<T> *data = m_data.load(std::memory_order_relaxed);

    if (data->size() - 1 < (back - front)) {
      circular_array<T> *new_data = data->resize(back, front);

      delete m_old.load(std::memory_order_relaxed);

      m_old.store(data, std::memory_order_relaxed);

      m_data.store(new_data, std::memory_order_relaxed);
    }

    data->push(back, item);
    m_back.store(back + 1, std::memory_order_release);
  }

  bool dequeue(T &item) {
    size_t back             = m_back.load(std::memory_order_relaxed) - 1;
    circular_array<T> *data = m_data.load(std::memory_order_relaxed);
    m_back.store(back, std::memory_order_relaxed);
    size_t front = m_front.load(std::memory_order_seq_cst);

    bool status = false;

    if (front <= back) {
      item   = data->pop(back);
      status = true;
      if (front == back) {
        if (!m_front.compare_exchange_strong(front, front + 1,
                                             std::memory_order_seq_cst,
                                             std::memory_order_relaxed)) {
          status = false;
        }
        m_back.store(back + 1, std::memory_order_relaxed);
      }
    } else {
      m_back.store(back + 1, std::memory_order_relaxed);
    }

    return status;
  }

  bool steal(T &item) {

    size_t front = m_front.load(std::memory_order_acquire);
    size_t back  = m_back.load(std::memory_order_seq_cst);

    if (front < back) {
      circular_array<T> *data = m_data.load(std::memory_order_consume);
      item                    = data->pop(front);
      if (!m_front.compare_exchange_strong(front, front + 1,
                                           std::memory_order_seq_cst,
                                           std::memory_order_relaxed)) {
        return false;
      }
    } else {
      return false;
    }

    return true;
  }
};

#endif