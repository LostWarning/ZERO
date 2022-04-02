#ifndef __CORO_TASK_HPP__
#define __CORO_TASK_HPP__

#include "Awaiters.hpp"
#include "scheduler/scheduler.hpp"

#include <algorithm>
#include <coroutine>

template <typename Return = void>
struct task {
  struct promise_type : public Awaiter_Transforms {
    std::coroutine_handle<> m_continuation;
    Return m_value;
    std::suspend_always initial_suspend() const noexcept { return {}; }
    auto final_suspend() noexcept {
      struct {
        std::coroutine_handle<> &m_continuation;
        constexpr bool await_ready() const noexcept { return false; }

        std::coroutine_handle<>
        await_suspend(const std::coroutine_handle<> &) noexcept {
          return m_continuation;
        }

        constexpr void await_resume() const noexcept {}
      } awaiter{m_continuation};

      return awaiter;
    }

    void return_value(const Return &value) noexcept {
      m_value = std::move(value);
    }

    void unhandled_exception() noexcept {}

    task<Return> get_return_object() noexcept { return task<Return>(this); }
  };

  promise_type *m_promise;
  task(promise_type *promise) : m_promise(promise) {}

  constexpr bool await_ready() const noexcept { return false; }

  auto await_suspend(const std::coroutine_handle<> &handle) const noexcept
      -> std::coroutine_handle<> {
    m_promise->m_continuation = handle;
    return std::coroutine_handle<promise_type>::from_promise(*m_promise);
  }

  constexpr Return await_resume() const noexcept { return m_promise->m_value; }

  void set_scheduler(scheduler *s) { this->m_promise->m_scheduler = s; }
};

template <>
struct task<void> {
  struct promise_type : public Awaiter_Transforms {
    std::coroutine_handle<> m_continuation;
    std::suspend_always initial_suspend() const noexcept { return {}; }
    auto final_suspend() noexcept {
      struct {
        std::coroutine_handle<> &m_continuation;
        constexpr bool await_ready() const noexcept { return false; }

        std::coroutine_handle<>
        await_suspend(const std::coroutine_handle<> &) noexcept {
          return m_continuation;
        }

        constexpr void await_resume() const noexcept {}
      } awaiter{m_continuation};

      return awaiter;
    }

    constexpr void return_void() const noexcept {}

    void unhandled_exception() noexcept {}

    task get_return_object() noexcept { return task(this); }
  };

  promise_type *m_promise;
  task(promise_type *promise) : m_promise(promise) {}

  constexpr bool await_ready() const noexcept { return false; }

  auto await_suspend(const std::coroutine_handle<> &handle) const noexcept
      -> std::coroutine_handle<> {
    m_promise->m_continuation = handle;
    return std::coroutine_handle<promise_type>::from_promise(*m_promise);
  }

  constexpr void await_resume() const noexcept {}

  void set_scheduler(scheduler *s) { this->m_promise->m_scheduler = s; }
};

#endif