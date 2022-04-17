#ifndef __COROUTINE_GENERATOR_HPP__
#define __COROUTINE_GENERATOR_HPP__

#include "Awaiters.hpp"
#include "scheduler/scheduler.hpp"

#include <coroutine>

template <typename Promise>
struct generator_promise {
  Promise *m_promise;
  constexpr bool await_ready() const noexcept { return false; }

  std::coroutine_handle<>
  await_suspend(const std::coroutine_handle<> &) const noexcept {
    return m_promise->m_continuation;
  }

  constexpr void await_resume() const noexcept {}
};

template <typename Promise>
struct generator_final_suspend {
  Promise *m_promise;
  constexpr bool await_ready() const noexcept { return false; }

  std::coroutine_handle<>
  await_suspend(const std::coroutine_handle<> &) const noexcept {
    if (m_promise->m_cancel_handle_ctl.exchange(true,
                                                std::memory_order_relaxed)) {
      m_promise->m_cancel_scheduler->schedule(m_promise->m_cancel_continuation);
    }
    return m_promise->m_continuation;
  }

  constexpr void await_resume() const noexcept {}
};

template <typename Promise, typename Return>
struct generator_future {
  Promise *m_promise;
  constexpr bool await_ready() const noexcept { return false; }

  auto await_suspend(const std::coroutine_handle<> &handle) const noexcept
      -> std::coroutine_handle<> {
    m_promise->m_continuation = handle;
    return std::coroutine_handle<Promise>::from_promise(*m_promise);
  }

  Return await_resume() const noexcept { return std::move(m_promise->m_value); }

  auto cancel() {
    this->m_promise->m_stop_source.request_stop();
    return *this;
  }
};

template <typename Ret>
struct generator {
  using Return = std::remove_reference_t<Ret>;
  struct promise_type : public Awaiter_Transforms {
    std::coroutine_handle<> m_continuation;
    Return m_value;

    std::suspend_always initial_suspend() const noexcept { return {}; }

    auto final_suspend() noexcept {
      return generator_final_suspend<promise_type>{this};
    }

    auto yield_value(const Return &value) {
      m_value = value;
      return generator_promise<promise_type>{this};
    }

    void return_value(const Return &value) noexcept {
      m_value = std::move(value);
    }

    void unhandled_exception() noexcept {}

    generator<Ret> get_return_object() noexcept { return generator<Ret>(this); }
  };

  promise_type *m_promise;
  generator(promise_type *promise) : m_promise(promise) {}

  generator(const generator &) = delete;
  generator &operator=(const generator &) = delete;

  generator(generator &&Other) {
    this->m_promise = Other.m_promise;
    Other.m_promise = nullptr;
  }
  generator &operator=(generator &&Other) {
    this->m_promise = Other.m_promise;
    Other.m_promise = nullptr;
    return *this;
  }

  auto operator co_await() {
    return generator_future<promise_type, Ret>{m_promise};
  }

  void via(scheduler *s) { this->m_promise->m_scheduler = s; }

  auto cancel() {
    this->m_promise->m_stop_source.request_stop();
    return generator_future<promise_type, Ret>{m_promise};
  }

  operator bool() {
    return !std::coroutine_handle<promise_type>::from_promise(*m_promise)
                .done();
  }
};

#endif