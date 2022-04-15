#ifndef __CORO_SCHEDULER_CORO_SCHEDULER_HPP__
#define __CORO_SCHEDULER_CORO_SCHEDULER_HPP__

#include "queue/work_stealing_queue.hpp"

#include <atomic>
#include <condition_variable>
#include <coroutine>
#include <functional>
#include <mutex>
#include <vector>

using task_queue = work_stealing_queue<std::coroutine_handle<>>;

struct thread_status {
  enum class STATUS { NEW, READY, RUNNING, SUSPENDED };
  std::atomic<STATUS> m_status;
};

struct scheduler_task {
  std::coroutine_handle<> m_handle;

  struct promise_type {

    auto initial_suspend() const noexcept { return std::suspend_always{}; }
    auto final_suspend() const noexcept { return std::suspend_never{}; }

    void return_void() {}

    void unhandled_exception() {}

    scheduler_task get_return_object() {
      return scheduler_task(
          std::coroutine_handle<promise_type>::from_promise(*this));
    }
  };

  scheduler_task(std::coroutine_handle<> handle) : m_handle(handle) {}

  std::coroutine_handle<> handle() { return m_handle; }
};

struct thread_context {
  std::thread m_thread;
  thread_status m_thread_status;
  std::coroutine_handle<> m_waiting_channel;
  task_queue *m_tasks;
};

class scheduler {
  static thread_local unsigned int m_thread_id;
  static thread_local unsigned int m_coro_scheduler_id;
  static unsigned int m_coro_scheduler_count;
  unsigned int m_id = 0;

  std::atomic_uint m_total_threads{0};
  std::atomic_uint m_total_suspended_threads{0};
  std::atomic_uint m_total_ready_threads{0};
  std::atomic_uint m_total_running_threads{0};

  std::vector<thread_context *> m_thread_cxts;

  std::mutex m_task_mutex;

  std::mutex m_spawn_thread_mutex;
  std::mutex m_global_task_queue_mutex;
  std::atomic_flag m_task_wait_flag;

  bool m_stop_requested = false;

public:
  scheduler();
  scheduler(unsigned int threa);
  ~scheduler();

  void schedule(const std::coroutine_handle<> &handle) noexcept;

  auto get_next_coroutine() noexcept -> std::coroutine_handle<>;

  void spawn_workers(const unsigned int &count);

protected:
  void init_thread();
  void set_thread_suspended() noexcept;
  void set_thread_ready() noexcept;
  void set_thread_running() noexcept;
  void set_thread_status(thread_status::STATUS status) noexcept;

  scheduler_task awaiter();
  std::coroutine_handle<> get_waiting_channel() noexcept;
  bool peek_next_coroutine(std::coroutine_handle<> &handle) noexcept;
  bool steal_task(std::coroutine_handle<> &handle) noexcept;
};

#endif