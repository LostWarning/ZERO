#ifndef __QUEUE_CIRCULAR_ARRAY_HPP__
#define __QUEUE_CIRCULAR_ARRAY_HPP__

#include <atomic>

template <typename T>
class circular_array {
  size_t m_capacity;
  size_t m_mask;
  std::atomic<T> *m_data = nullptr;

public:
  explicit circular_array(size_t capacity)
      : m_capacity{capacity}
      , m_mask{m_capacity - 1}
      , m_data{new std::atomic<T>[m_capacity]} {}

  ~circular_array() { delete[] m_data; }

  template <typename D>
  void push(size_t i, D &&data) noexcept {
    m_data[i & m_mask].store(std::forward<D>(data), std::memory_order_relaxed);
  }

  T pop(size_t i) const noexcept {
    return m_data[i & m_mask].load(std::memory_order_relaxed);
  }

  circular_array *resize(size_t back, size_t front) {
    circular_array *ptr = new circular_array(2 * m_capacity);
    for (size_t i = front; i != back; ++i) {
      ptr->push(i, pop(i));
    }
    return ptr;
  }

  size_t size() const noexcept { return m_capacity; }
};

#endif