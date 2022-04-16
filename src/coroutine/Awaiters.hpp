#ifndef __COROUTINE_AWAITER_HPP__
#define __COROUTINE_AWAITER_HPP__

#include "scheduler/scheduler.hpp"

#include <concepts>
#include <stop_token>

template <typename T>
concept Resume_VIA = requires(T a, scheduler *s) {
  {a.via(s)};
};

template <typename T>
concept Schedule_ON = requires(T a, scheduler *s) {
  {a.schedule_on(s)};
};

template <typename T>
concept Task = requires(T a, scheduler *s) {
  {a.set_scheduler(s)};
};

struct get_scheduler {
  scheduler *m_scheduler;
  constexpr bool await_ready() const noexcept { return true; }
  constexpr bool await_suspend(const std::coroutine_handle<> &) const noexcept {
    return false;
  }
  scheduler *await_resume() const noexcept { return m_scheduler; }
};

struct get_stop_token {
  std::stop_source *m_stop_source;
  constexpr bool await_ready() const noexcept { return true; }
  constexpr bool await_suspend(const std::coroutine_handle<> &) const noexcept {
    return false;
  }
  std::stop_token await_resume() const noexcept {
    return m_stop_source->get_token();
  }
};

template <typename Promise>
struct cancel_awaiter {
  Promise *m_promise;
  bool await_ready() const noexcept {
    return m_promise->m_cancel_handle_ctl.load(std::memory_order_relaxed);
  }

  auto await_suspend(const std::coroutine_handle<> &handle) noexcept {
    m_promise->m_cancel_continuation = handle;

    return m_promise->m_cancel_handle_ctl.exchange(true,
                                                   std::memory_order_acq_rel)
               ? handle
               : m_promise->m_scheduler->get_next_coroutine();
  }

  void await_resume() const noexcept {}
};

struct Awaiter_Transforms {
  scheduler *m_scheduler{nullptr};
  scheduler *m_cancel_scheduler{nullptr};
  std::coroutine_handle<> m_cancel_continuation;
  std::stop_source m_stop_source;
  std::atomic_bool m_cancel_handle_ctl{false};
  template <Resume_VIA A>
  A &await_transform(A &awaiter) {
    awaiter.via(m_scheduler);
    return awaiter;
  }

  template <Resume_VIA A>
  A &&await_transform(A &&awaiter) {
    awaiter.via(m_scheduler);
    return std::move(awaiter);
  }

  template <Schedule_ON A>
  A &await_transform(A &awaiter) {
    awaiter.schedule_on(m_scheduler);
    return awaiter;
  }

  template <Schedule_ON A>
  A &&await_transform(A &&awaiter) {
    awaiter.schedule_on(m_scheduler);
    return std::move(awaiter);
  }

  template <Task A>
  A &await_transform(A &awaiter) {
    awaiter.set_scheduler(m_scheduler);
    return awaiter;
  }

  template <Task A>
  A &&await_transform(A &&awaiter) {
    awaiter.set_scheduler(m_scheduler);
    return std::move(awaiter);
  }

  get_scheduler &await_transform(get_scheduler &gs) {
    gs.m_scheduler = m_scheduler;
    return gs;
  }

  get_scheduler &&await_transform(get_scheduler &&gs) {
    gs.m_scheduler = m_scheduler;
    return std::move(gs);
  }

  get_stop_token &await_transform(get_stop_token &st) {
    st.m_stop_source = &m_stop_source;
    return st;
  }

  get_stop_token &&await_transform(get_stop_token &&st) {
    st.m_stop_source = &m_stop_source;
    return std::move(st);
  }

  template <typename T>
  cancel_awaiter<T> &await_transform(cancel_awaiter<T> &ca) {
    ca.m_promise->m_cancel_scheduler = m_scheduler;
    return ca;
  }

  template <typename T>
  cancel_awaiter<T> &&await_transform(cancel_awaiter<T> &&ca) {
    ca.m_promise->m_cancel_scheduler = m_scheduler;
    return std::move(ca);
  }

  template <typename Default>
  Default &&await_transform(Default &&d) {
    return static_cast<Default &&>(d);
  }
};

#endif