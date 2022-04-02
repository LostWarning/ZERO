#ifndef __QUEUE_SHARED_WORK_QUEUE_HPP__
#define __QUEUE_SHARED_WORK_QUEUE_HPP__

#include "circular_array.hpp"

#include <atomic>

template <typename T>
class io_work_queue {

  std::atomic<size_t> m_front;
  std::atomic<size_t> m_back;
  std::atomic<circular_array<T> *> m_data{nullptr};
  std::atomic<circular_array<T> *> m_old{nullptr};

public:
  explicit io_work_queue(size_t capacity) {
    m_front.store(0, std::memory_order_relaxed);
    m_back.store(0, std::memory_order_relaxed);
    m_data.store(new circular_array<T>(capacity), std::memory_order_relaxed);
  }

  ~io_work_queue() { delete m_data.load(std::memory_order_relaxed); }

  bool empty() const noexcept {
    size_t back  = m_back.load(std::memory_order_relaxed);
    size_t front = m_front.load(std::memory_order_relaxed);
    return back == front;
  }

  void enqueue(T &&item) {
    size_t back             = m_back.load(std::memory_order_relaxed);
    size_t front            = m_front.load(std::memory_order_acquire);
    circular_array<T> *data = m_data.load(std::memory_order_relaxed);

    if (data->size() - 1 < static_cast<size_t>(back - front)) {
      if (back < front) {
        size_t a  = 0;
        auto size = (back + ((a - 1) - front)) + 1;
        if (data->size() - 1 < size) {
          resize(data, back, front);
        }
      } else {
        resize(data, back, front);
      }
    }

    data->push(back, item);
    m_back.store(back + 1, std::memory_order_release);
  }

  bool dequeue(T &item) {

    size_t back  = m_back.load(std::memory_order_relaxed);
    size_t front = m_front.load(std::memory_order_relaxed);

    if (back == front) {
      return false;
    }

    circular_array<T> *data = m_data.load(std::memory_order_consume);
    item                    = data->pop(front);
    m_front++;
    return true;
  }

protected:
  void resize(circular_array<T> *data, size_t back, size_t front) {
    circular_array<T> *new_data = data->resize(back, front);
    delete m_old.load(std::memory_order_relaxed);

    m_old.store(data, std::memory_order_relaxed);

    m_data.store(new_data, std::memory_order_relaxed);
  }
};

#endif