#ifndef __QUEUE_SHARED_WORK_QUEUE_HPP__
#define __QUEUE_SHARED_WORK_QUEUE_HPP__

#include "circular_array.hpp"

#include <atomic>

/* A first in first out lock free data structure
 */

template <typename T>
class io_work_queue {

  // Front of the queue
  std::atomic<size_t> m_front;

  // Back of the queue
  std::atomic<size_t> m_back;

  // Current buffer used for storing data
  std::atomic<circular_array<T> *> m_data{nullptr};

  /* Old buffer that was used for store data.
   * Keep the old one just in-case some thread is dequeuing when new buffer is
   * being created.
   */
  std::atomic<circular_array<T> *> m_old{nullptr};

public:
  // Create a io_work_queue with size capacity (capacity should be power of 2)
  explicit io_work_queue(size_t capacity) {
    m_front.store(0, std::memory_order_relaxed);
    m_back.store(0, std::memory_order_relaxed);
    m_data.store(new circular_array<T>(capacity), std::memory_order_relaxed);
  }

  ~io_work_queue() {
    delete m_data.load(std::memory_order_relaxed);
    delete m_old.load(std::memory_order_relaxed);
  }

  // Check the io_work_queue is empty.
  bool empty() const noexcept {
    size_t back  = m_back.load(std::memory_order_relaxed);
    size_t front = m_front.load(std::memory_order_relaxed);
    return back == front;
  }

  /* Add a new item to back of the queue. If the queue is full expand the queue
   * to next power of 2
   */
  void enqueue(T &&item) {
    size_t back             = m_back.load(std::memory_order_relaxed);
    size_t front            = m_front.load(std::memory_order_acquire);
    circular_array<T> *data = m_data.load(std::memory_order_relaxed);

    // Check the queue is full and resize the array.
    if (data->size() - 1 < static_cast<size_t>(back - front)) [[unlikely]] {
      if (back < front) {
        size_t a  = 0;
        auto size = (back + ((a - 1) - front)) + 1;
        if (data->size() - 1 < size) {
          data = resize(data, back, front);
        }
      } else {
        data = resize(data, back, front);
      }
    }

    data->push(back, std::forward<T>(item));
    m_back.store(back + 1, std::memory_order_release);
  }

  /* Add a new items to back of the queue. If the queue is full expand the queue
   * to next power of 2
   */
  void bulk_enqueue(std::vector<T> &items) {
    size_t items_count      = items.size();
    size_t back             = m_back.load(std::memory_order_relaxed);
    size_t front            = m_front.load(std::memory_order_acquire);
    circular_array<T> *data = m_data.load(std::memory_order_relaxed);

    if ((data->size() - 1 - items_count) < static_cast<size_t>(back - front))
        [[unlikely]] {
      if (back < front) {
        size_t a  = 0;
        auto size = (back + ((a - 1) - front)) + 1;
        if ((data->size() - 1 - items_count) < size) {
          data = resize(data, back, front);
        }
      } else {
        data = resize(data, back, front);
      }
    }
    for (size_t i = 0; i < items_count; ++i) {
      data->push(back + i, items[i]);
    }
    m_back.store(back + 1 + items_count, std::memory_order_release);
  }

  /* Pop an item from front of the queue.
   */
  bool dequeue(T &item) {

    size_t back  = m_back.load(std::memory_order_relaxed);
    size_t front = m_front.load(std::memory_order_relaxed);

    if (back == front) {
      return false;
    }

    circular_array<T> *data = m_data.load(std::memory_order_consume);
    item                    = data->pop(front);
    m_front.store(m_front + 1, std::memory_order_seq_cst);
    return true;
  }

protected:
  circular_array<T> *resize(circular_array<T> *data, size_t back,
                            size_t front) {
    circular_array<T> *new_data = data->resize(back, front);
    delete m_old.load(std::memory_order_relaxed);
    m_old.store(data, std::memory_order_relaxed);
    m_data.store(new_data, std::memory_order_relaxed);
    return new_data;
  }
};

#endif