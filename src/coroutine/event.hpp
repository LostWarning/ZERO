#ifndef __COROUTINE_EVENTS_HPP
#define __COROUTINE_EVENTS_HPP

#include "scheduler/scheduler.hpp"

#include <atomic>
#include <coroutine>

class event {
  std::coroutine_handle<> m_handle;
  scheduler *m_scheduler             = nullptr;
  std::atomic_uint64_t m_event_count = 0;
  std::atomic_bool m_handle_ctl      = false;

  struct event_awaiter {
    event *m_promise;

    bool await_ready() const noexcept {
      return m_promise->m_event_count.load(std::memory_order_relaxed);
    }

    auto await_suspend(const std::coroutine_handle<> &handle) noexcept {
      m_promise->m_handle = handle;
      m_promise->m_handle_ctl.store(true, std::memory_order_relaxed);
      if (m_promise->m_event_count.load(std::memory_order_relaxed)) {
        if (m_promise->m_handle_ctl.exchange(false,
                                             std::memory_order_relaxed)) {
          return handle;
        }
      }
      return m_promise->m_scheduler->get_next_coroutine();
    }

    auto await_resume() noexcept {
      return m_promise->m_event_count.exchange(0, std::memory_order_relaxed);
    }
  };

public:
  auto operator co_await() { return event_awaiter{this}; }

  auto set(uint64_t events_count) {
    m_event_count.fetch_add(events_count, std::memory_order_relaxed);
    bool expected = true;
    if (m_handle_ctl.compare_exchange_strong(expected, false,
                                             std::memory_order_acquire)) {
      m_scheduler->schedule(m_handle);
      return;
    }
  }

  void via(scheduler *s) { this->m_scheduler = s; }
};

#endif