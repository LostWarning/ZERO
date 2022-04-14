#ifndef __CORO_TASK_HPP__
#define __CORO_TASK_HPP__

#include "Awaiters.hpp"
#include "scheduler/scheduler.hpp"

#include <algorithm>
#include <coroutine>

template <typename Promise, typename Return = void>
struct task_awaiter {
  Promise *m_promise;
  constexpr bool await_ready() const noexcept { return false; }

  auto await_suspend(const std::coroutine_handle<> &handle) const noexcept
      -> std::coroutine_handle<> {
    m_promise->m_continuation = handle;
    return std::coroutine_handle<Promise>::from_promise(*m_promise);
  }

  constexpr Return await_resume() const noexcept { return m_promise->m_value; }

  auto cancel() { return cancel_awaiter<Promise>(m_promise); }
};

template <typename Promise>
struct task_awaiter<Promise, void> {
  Promise *m_promise;
  constexpr bool await_ready() const noexcept { return false; }

  auto await_suspend(const std::coroutine_handle<> &handle) const noexcept
      -> std::coroutine_handle<> {
    m_promise->m_continuation = handle;
    return std::coroutine_handle<Promise>::from_promise(*m_promise);
  }

  constexpr void await_resume() const noexcept {}

  auto cancel() { return cancel_awaiter<Promise>(m_promise); }
};

template <typename Promise>
struct task_final_awaiter {
  Promise *m_promise;
  constexpr bool await_ready() const noexcept { return false; }

  std::coroutine_handle<>
  await_suspend(const std::coroutine_handle<> &) noexcept {
    if (m_promise->m_cancel_handle_ctl.exchange(true,
                                                std::memory_order_relaxed)) {
      m_promise->m_scheduler->schedule(m_promise->m_cancel_continuation);
    }
    return m_promise->m_continuation;
  }

  constexpr void await_resume() const noexcept {}
};

template <typename Return = void>
struct task {
  struct promise_type : public Awaiter_Transforms {
    std::coroutine_handle<> m_continuation;
    Return m_value;
    std::suspend_always initial_suspend() const noexcept { return {}; }
    auto final_suspend() noexcept {
      return task_final_awaiter<promise_type>(this);
    }

    void return_value(const Return &value) noexcept {
      m_value = std::move(value);
    }

    void unhandled_exception() noexcept {}

    task<Return> get_return_object() noexcept { return task<Return>(this); }
  };

  promise_type *m_promise;
  task(promise_type *promise) : m_promise(promise) {}

  task(const task &) = delete;
  task &operator=(const task &) = delete;

  task(task &&Other) {
    this->m_promise = Other.m_promise;
    Other.m_promise = nullptr;
  }
  task &operator=(task &&Other) {
    this->m_promise = Other.m_promise;
    Other.m_promise = nullptr;
    return *this;
  }

  auto operator co_await() {
    return task_awaiter<promise_type, Return>{m_promise};
  }

  void set_scheduler(scheduler *s) { this->m_promise->m_scheduler = s; }

  auto cancel() { return cancel_awaiter<promise_type>(m_promise); }
};

template <>
struct task<void> {
  struct promise_type : public Awaiter_Transforms {
    std::coroutine_handle<> m_continuation;
    std::suspend_always initial_suspend() const noexcept { return {}; }
    auto final_suspend() noexcept {
      return task_final_awaiter<promise_type>(this);
    }

    constexpr void return_void() const noexcept {}

    void unhandled_exception() noexcept {}

    task get_return_object() noexcept { return task(this); }
  };

  promise_type *m_promise;
  task(promise_type *promise) : m_promise(promise) {}

  task(const task &) = delete;
  task &operator=(const task &) = delete;

  task(task &&Other) {
    this->m_promise = Other.m_promise;
    Other.m_promise = nullptr;
  }
  task &operator=(task &&Other) {
    this->m_promise = Other.m_promise;
    Other.m_promise = nullptr;
    return *this;
  }

  auto operator co_await() {
    return task_awaiter<promise_type, void>{m_promise};
  }

  void set_scheduler(scheduler *s) { this->m_promise->m_scheduler = s; }

  auto cancel() { return cancel_awaiter<promise_type>(m_promise); }
};

#endif