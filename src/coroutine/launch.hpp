#ifndef __CORO_LAUNCH_HPP__
#define __CORO_LAUNCH_HPP__

#include "Awaiters.hpp"
#include "scheduler/scheduler.hpp"

#include <algorithm>
#include <atomic>
#include <coroutine>
#include <future>
#include <iostream>
#include <type_traits>

struct launch_final_awaiter {
  std::atomic_bool &m_destroy_ctl;
  constexpr bool await_ready() const noexcept { return false; }

  void await_suspend(std::coroutine_handle<> handle) noexcept {
    if (m_destroy_ctl.exchange(true, std::memory_order_relaxed)) {
      handle.destroy();
    }
  }
  constexpr void await_resume() const noexcept {}
};

template <typename Return = void>
struct launch {
  struct promise_type : public Awaiter_Transforms {
    std::promise<Return> m_result;
    std::atomic_bool m_destroy_ctl;

    std::suspend_always initial_suspend() noexcept { return {}; }
    launch_final_awaiter final_suspend() noexcept { return {m_destroy_ctl}; }

    void return_value(const Return &value) {
      m_result.set_value(std::move(value));
    }

    void unhandled_exception() {}

    launch<Return> get_return_object() { return {this}; }
  };

  launch(promise_type *promise) : m_promise(promise) {}

  launch(const launch &) = delete;
  launch &operator=(const launch &) = delete;

  launch(launch &&l) {
    this->m_promise = l.m_promise;
    l.m_promise     = nullptr;
  }
  launch &operator=(launch &&Other) {
    this->m_promise = Other.m_promise;
    Other.m_promise = nullptr;
    return *this;
  }

  ~launch() {
    if (m_promise != nullptr &&
        m_promise->m_destroy_ctl.exchange(true, std::memory_order_relaxed)) {
      std::coroutine_handle<promise_type>::from_promise(*m_promise).destroy();
    }
  }

  operator Return() const noexcept {
    return m_promise->m_result.get_future().get();
  }

  launch<Return> &&schedule_on(scheduler *schd) {
    m_promise->m_scheduler = schd;
    schd->schedule(
        std::coroutine_handle<promise_type>::from_promise(*m_promise));
    return std::move(*this);
  }

private:
  promise_type *m_promise;
};

template <>
struct launch<void> {
  struct promise_type : public Awaiter_Transforms {
    std::promise<int> m_result;
    std::atomic_bool m_destroy_ctl;

    std::suspend_always initial_suspend() noexcept { return {}; }
    launch_final_awaiter final_suspend() noexcept { return {m_destroy_ctl}; }

    void return_void() { m_result.set_value(0); }

    void unhandled_exception() {}

    launch get_return_object() { return launch(this); }
  };

  launch(promise_type *promise) : m_promise{promise} {}

  launch(const launch &) = delete;
  launch &operator=(const launch &) = delete;

  launch(launch &&l) {
    this->m_promise = l.m_promise;
    l.m_promise     = nullptr;
  }
  launch &operator=(launch &&Other) {
    this->m_promise = Other.m_promise;
    Other.m_promise = nullptr;
    return *this;
  }

  ~launch() {
    if (m_promise != nullptr &&
        m_promise->m_destroy_ctl.exchange(true, std::memory_order_relaxed)) {
      std::coroutine_handle<promise_type>::from_promise(*m_promise).destroy();
    }
  }

  void join() { m_promise->m_result.get_future().get(); }

  launch<void> &&schedule_on(scheduler *schd) {
    m_promise->m_scheduler = schd;
    schd->schedule(
        std::coroutine_handle<promise_type>::from_promise(*m_promise));
    return std::move(*this);
  }

private:
  promise_type *m_promise;
};

#endif