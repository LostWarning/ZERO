#ifndef __IO_URING_DATA_HPP__
#define __IO_URING_DATA_HPP__

#include "coroutine/scheduler/scheduler.hpp"
#include "queue/pool_allocator.hpp"

#include <atomic>
#include <coroutine>
#include <liburing.h>

struct uring_data {
  using allocator = pool_allocator<uring_data, 128>;

  scheduler *m_scheduler = nullptr;
  allocator *m_allocator = nullptr;
  std::coroutine_handle<> m_handle;
  int m_result         = 0;
  unsigned int m_flags = 0;
  std::atomic_bool m_handle_ctl{false};
  std::atomic_bool m_destroy_ctl{false};

  void destroy() { m_allocator->deallocate(this); }
};

class uring_awaiter {
  uring_data *m_data;

public:
  uring_awaiter(const uring_awaiter &) = delete;
  uring_awaiter &operator=(const uring_awaiter &) = delete;

  uring_awaiter(uring_awaiter &&Other) {
    m_data = Other.m_data;

    Other.m_data = nullptr;
  }

  uring_awaiter &operator=(uring_awaiter &&Other) {
    m_data = Other.m_data;

    Other.m_data = nullptr;

    return *this;
  }

  auto operator co_await() {
    struct {
      uring_data *m_data = nullptr;
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
      auto await_resume() const noexcept {
        // Acquire the changes to the result
        m_data->m_handle_ctl.load();
        struct {
          int result;
          unsigned int flags;
          operator int() { return result; }
        } result{m_data->m_result, m_data->m_flags};
        return result;
      }
    } awaiter{m_data};

    return awaiter;
  }

  uring_awaiter(uring_data::allocator *allocator) {
    m_data              = allocator->allocate();
    m_data->m_allocator = allocator;
  }

  ~uring_awaiter() {
    if (m_data != nullptr &&
        m_data->m_destroy_ctl.exchange(true, std::memory_order_relaxed)) {
      m_data->destroy();
    }
  }

  uring_data *get_data() const noexcept { return m_data; }

  void via(scheduler *s) { this->m_data->m_scheduler = s; }
};

#endif