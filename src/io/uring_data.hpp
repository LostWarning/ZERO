#ifndef __IO_URING_DATA_HPP__
#define __IO_URING_DATA_HPP__

#include "coroutine/scheduler/scheduler.hpp"
#include "queue/pool_allocator.hpp"

#include <atomic>
#include <coroutine>
#include <liburing.h>

struct uring_data {
  scheduler *m_scheduler = nullptr;
  std::coroutine_handle<> m_handle;
  int m_result;
  unsigned int m_flags;
  std::atomic_bool m_handle_ctl{false};
};

using uring_allocator = pool_allocator<uring_data, 102>;

class uring_awaiter {
  uring_data *m_data;
  uring_allocator *m_allocator = nullptr;

public:
  uring_awaiter(const uring_awaiter &) = delete;

  uring_awaiter(uring_awaiter &&Other) {
    m_data      = Other.m_data;
    m_allocator = Other.m_allocator;

    Other.m_data      = nullptr;
    Other.m_allocator = nullptr;
  }

  bool await_ready() const noexcept {
    return m_data->m_handle_ctl.load(std::memory_order_relaxed);
  }

  auto await_suspend(const std::coroutine_handle<> &handle) noexcept
      -> std::coroutine_handle<> {
    m_data->m_handle = handle;
    auto schd        = m_data->m_scheduler;
    if (m_data->m_handle_ctl.exchange(true, std::memory_order_acq_rel)) {
      return handle;
    }
    return schd->get_next_coroutine();
  }
  int await_resume() const noexcept {
    // Acquire the changes to the result
    m_data->m_handle_ctl.load(std::memory_order_acq_rel);
    return m_data->m_result;
  }

  uring_awaiter(uring_allocator *allocator) {
    m_allocator = allocator;
    m_data      = m_allocator->allocate();
  }

  ~uring_awaiter() {
    if (m_allocator != nullptr) {
      m_allocator->deallocate(m_data);
    }
  }

  uring_data *get_data() const noexcept { return m_data; }

  void via(scheduler *s) { this->m_data->m_scheduler = s; }
};

#endif