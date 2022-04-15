#ifndef __CORO_ASYNC_HPP__
#define __CORO_ASYNC_HPP__

#include "Awaiters.hpp"
#include "io/io_service.hpp"
#include "scheduler/scheduler.hpp"

#include <atomic>
#include <coroutine>
#include <functional>
#include <sys/timerfd.h>

template <typename Promise, typename Return = void>
struct async_awaiter {
  Promise *m_promise;
  bool await_ready() const noexcept {
    return m_promise->m_handle_ctl.load(std::memory_order_acquire);
  }

  auto await_suspend(const std::coroutine_handle<> &handle) noexcept
      -> std::coroutine_handle<> {
    m_promise->m_continuation = handle;
    if (m_promise->m_handle_ctl.exchange(true, std::memory_order_acq_rel)) {
      return handle;
    }
    return m_promise->m_scheduler->get_next_coroutine();
  }

  auto await_resume() const noexcept -> Return & { return m_promise->m_value; }

  auto cancel() {
    m_promise->m_stop_source.request_stop();
    return cancel_awaiter<Promise>(m_promise);
  }
};

template <typename Promise>
struct async_awaiter<Promise, void> {
  Promise *m_promise;
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

  void await_resume() const noexcept {}

  auto cancel() {
    m_promise->m_stop_source.request_stop();
    return cancel_awaiter<Promise>(m_promise);
  }
};

template <typename Promise>
struct async_final_suspend {
  Promise *m_promise;
  constexpr bool await_ready() const noexcept { return false; }

  auto await_suspend(const std::coroutine_handle<> &handle) const noexcept
      -> std::coroutine_handle<> {

    if (m_promise->m_cancel_handle_ctl.exchange(true,
                                                std::memory_order_acquire)) {
      m_promise->m_scheduler->schedule(m_promise->m_cancel_continuation);
    }

    auto continuation =
        m_promise->m_handle_ctl.exchange(true, std::memory_order_acq_rel)
            ? m_promise->m_continuation
            : m_promise->m_scheduler->get_next_coroutine();

    if (m_promise->m_destroy_ctl.exchange(true, std::memory_order_relaxed)) {
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
    Return m_value;
    std::coroutine_handle<> m_continuation;
    std::atomic_bool m_handle_ctl{false};
    std::atomic_bool m_destroy_ctl{false};

    std::suspend_always initial_suspend() const noexcept { return {}; }

    auto final_suspend() noexcept { return async_final_suspend{this}; }

    void return_value(const Return &value) noexcept {
      m_value = std::move(value);
    }

    void unhandled_exception() {}

    async get_return_object() { return async(this); }
  };

  promise_type *m_promise;
  async(promise_type *promise) : m_promise(promise) {}

  async(const async &) = delete;
  async operator=(const async &) = delete;

  async(async &&Other) {
    this->m_promise = Other.m_promise;
    Other.m_promise = nullptr;
  }
  async &operator=(async &&Other) {
    this->m_promise = Other.m_promise;
    Other.m_promise = nullptr;
    return *this;
  }

  ~async() {
    if (m_promise != nullptr &&
        m_promise->m_destroy_ctl.exchange(true, std::memory_order_relaxed)) {
      std::coroutine_handle<promise_type>::from_promise(*m_promise).destroy();
    }
  }

  auto operator co_await() {
    return async_awaiter<promise_type, Return>{m_promise};
  }

  auto schedule_on(scheduler *s) {
    if (this->m_promise->m_scheduler == nullptr) {
      this->m_promise->m_scheduler = s;
      s->schedule(
          std::coroutine_handle<promise_type>::from_promise(*m_promise));
    }
    return async_awaiter<promise_type, Return>{m_promise};
  }

  auto cancel() {
    m_promise->m_stop_source.request_stop();
    return cancel_awaiter<promise_type>(m_promise);
  }
};

template <>
struct async<void> {
  using Return = void;
  struct promise_type : public Awaiter_Transforms {
    std::coroutine_handle<> m_continuation;
    std::atomic_bool m_handle_ctl{false};
    std::atomic_bool m_destroy_ctl{false};

    std::suspend_always initial_suspend() const noexcept { return {}; }

    auto final_suspend() noexcept { return async_final_suspend{this}; }

    void return_void() const noexcept {}

    void unhandled_exception() {}

    async get_return_object() { return async(this); }
  };

  promise_type *m_promise;
  async(promise_type *promise) : m_promise(promise) {}

  async(const async &) = delete;
  async &operator=(const async &) = delete;

  async(async &&Other) {
    this->m_promise = Other.m_promise;
    Other.m_promise = nullptr;
  }
  async &operator=(async &&Other) {
    this->m_promise = Other.m_promise;
    Other.m_promise = nullptr;
    return *this;
  }

  ~async() {
    if (m_promise != nullptr &&
        m_promise->m_destroy_ctl.exchange(true, std::memory_order_relaxed)) {
      std::coroutine_handle<promise_type>::from_promise(*m_promise).destroy();
    }
  }

  auto operator co_await() {
    return async_awaiter<promise_type, void>{m_promise};
  }

  auto schedule_on(scheduler *s) {
    if (this->m_promise->m_scheduler == nullptr) {
      this->m_promise->m_scheduler = s;
      s->schedule(
          std::coroutine_handle<promise_type>::from_promise(*m_promise));
    }
    return async_awaiter<promise_type, void>{m_promise};
  }

  auto cancel() {
    m_promise->m_stop_source.request_stop();
    return cancel_awaiter<promise_type>(m_promise);
  }
};

// async<void> timer(io_service *io, itimerspec spec, std::function<void()>
// func);

template <typename Awaitable>
async<void> timer(io_service *io, itimerspec spec, Awaitable awaitable) {
  int tfd = timerfd_create(CLOCK_REALTIME, 0);
  timerfd_settime(tfd, 0, &spec, NULL);
  std::stop_token st = co_await get_stop_token();
  std::stop_callback scb(st, [&] { io->close(tfd); });

  while (!st.stop_requested()) {
    std::cerr << "Waiting for timer to trigger\n";
    unsigned long u;
    co_await io->read(tfd, &u, sizeof(u), 0);
    co_await awaitable();
  }
  co_await io->close(tfd);
}

template <typename Awaitable>
async<typename Awaitable::Return> delayed(io_service *io, unsigned long sec,
                                          unsigned long nsec,
                                          Awaitable awaitable) {
  int tfd = timerfd_create(CLOCK_REALTIME, 0);
  itimerspec spec;
  spec.it_value.tv_sec     = sec;
  spec.it_value.tv_nsec    = nsec;
  spec.it_interval.tv_sec  = 0;
  spec.it_interval.tv_nsec = 0;
  timerfd_settime(tfd, 0, &spec, NULL);

  std::stop_token st = co_await get_stop_token();
  std::stop_callback scb(st, [&] { io->close(tfd); });

  unsigned long u;
  co_await io->read(tfd, &u, sizeof(u), 0);
  co_await io->close(tfd);
  co_return co_await awaitable;
}

template <typename Return>
async<Return> delayed(io_service *io, unsigned long sec, unsigned long nsec,
                      std::function<Return()> func) {
  int tfd = timerfd_create(CLOCK_REALTIME, 0);
  itimerspec spec;
  spec.it_value.tv_sec     = sec;
  spec.it_value.tv_nsec    = nsec;
  spec.it_interval.tv_sec  = 0;
  spec.it_interval.tv_nsec = 0;
  timerfd_settime(tfd, 0, &spec, NULL);

  std::stop_token st = co_await get_stop_token();
  std::stop_callback scb(st, [&] { io->close(tfd); });

  unsigned long u;
  co_await io->read(tfd, &u, sizeof(u), 0);
  co_await io->close(tfd);
  co_return func();
}

#endif