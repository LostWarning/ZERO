#ifndef __CORO_ASYNC_HPP__
#define __CORO_ASYNC_HPP__

#include "Awaiters.hpp"
#include "scheduler/scheduler.hpp"

#include <atomic>
#include <coroutine>

struct async_final_suspend {
  std::coroutine_handle<> &m_continuation;
  scheduler *m_scheduler;
  std::atomic_bool &m_handle_ctl;
  std::atomic_bool &m_destroy_ctl;
  constexpr bool await_ready() const noexcept { return false; }

  auto await_suspend(const std::coroutine_handle<> &handle) const noexcept
      -> std::coroutine_handle<> {

    auto continuation = m_handle_ctl.exchange(true, std::memory_order_acq_rel)
                            ? m_continuation
                            : m_scheduler->get_next_coroutine();

    if (m_destroy_ctl.exchange(true, std::memory_order_relaxed)) {
      handle.destroy();
    }

    return continuation;
  }

  constexpr void await_resume() const noexcept {}
};

template <typename Ret = void>
struct async {
  using Return = std::remove_reference<Ret>::type;
  struct promise_type : public Awaiter_Transforms {
    std::coroutine_handle<> m_continuation;
    std::atomic_bool m_handle_ctl{false};
    std::atomic_bool m_destroy_ctl{false};
    Return m_value;

    std::suspend_always initial_suspend() const noexcept { return {}; }

    auto final_suspend() noexcept {
      return async_final_suspend{m_continuation, m_scheduler, m_handle_ctl,
                                 m_destroy_ctl};
    }

    void return_value(const Return &value) noexcept {
      m_value = std::move(value);
    }

    void unhandled_exception() {}

    async get_return_object() { return async(this); }

    get_scheduler &await_transform(get_scheduler &s) {
      s.m_scheduler = m_scheduler;
      return s;
    }

    get_scheduler &&await_transform(get_scheduler &&s) {
      s.m_scheduler = m_scheduler;
      return std::move(s);
    }

    template <Resume_VIA A>
    A &&await_transform(A &&awaiter) {
      awaiter.via(m_scheduler);
      return std::move(awaiter);
    }
  };

  promise_type *m_promise;
  async(promise_type *promise) : m_promise(promise) {}

  async(const async &) = delete;

  async(async &&Other) {
    this->m_promise = Other.m_promise;
    Other.m_promise = nullptr;
  }

  ~async() {
    if (m_promise != nullptr &&
        m_promise->m_destroy_ctl.exchange(true, std::memory_order_relaxed)) {
      std::coroutine_handle<promise_type>::from_promise(*m_promise).destroy();
    }
  }

  bool await_ready() const noexcept {
    return m_promise->m_handle_ctl.load(std::memory_order_acquire);
  }

  auto await_suspend(const std::coroutine_handle<> &handle) const noexcept
      -> std::coroutine_handle<> {
    m_promise->m_continuation = handle;
    if (m_promise->m_handle_ctl.exchange(true, std::memory_order_acq_rel)) {
      return handle;
    }
    return m_promise->m_scheduler->get_next_coroutine();
  }

  auto await_resume() const noexcept -> Return & { return m_promise->m_value; }

  void schedule_on(scheduler *s) {
    this->m_promise->m_scheduler = s;
    s->schedule(std::coroutine_handle<promise_type>::from_promise(*m_promise));
  }
};

template <>
struct async<void> {
  struct promise_type : public Awaiter_Transforms {
    std::coroutine_handle<> m_continuation;
    std::atomic_bool m_handle_ctl{false};
    std::atomic_bool m_destroy_ctl{false};

    std::suspend_always initial_suspend() const noexcept { return {}; }

    auto final_suspend() noexcept {
      return async_final_suspend{m_continuation, m_scheduler, m_handle_ctl,
                                 m_destroy_ctl};
    }

    void return_void() const noexcept {}

    void unhandled_exception() {}

    async get_return_object() { return async(this); }
  };

  promise_type *m_promise;
  async(promise_type *promise) : m_promise(promise) {}

  async(const async &) = delete;

  async(async &&Other) {
    this->m_promise = Other.m_promise;
    Other.m_promise = nullptr;
  }
  ~async() {
    if (m_promise != nullptr &&
        m_promise->m_destroy_ctl.exchange(true, std::memory_order_relaxed)) {
      std::coroutine_handle<promise_type>::from_promise(*m_promise).destroy();
    }
  }

  bool await_ready() const noexcept {
    return m_promise->m_handle_ctl.load(std::memory_order_relaxed);
  }

  auto await_suspend(const std::coroutine_handle<> &handle) const noexcept
      -> std::coroutine_handle<> {
    m_promise->m_continuation = handle;
    if (m_promise->m_handle_ctl.exchange(true, std::memory_order_acq_rel)) {
      return handle;
    }
    return m_promise->m_scheduler->get_next_coroutine();
  }

  constexpr void await_resume() const noexcept {}

  void schedule_on(scheduler *s) {
    this->m_promise->m_scheduler = s;
    s->schedule(std::coroutine_handle<promise_type>::from_promise(*m_promise));
  }
};

#endif