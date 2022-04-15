#include "scheduler.hpp"
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

thread_local unsigned int scheduler::m_thread_id         = 0;
thread_local unsigned int scheduler::m_coro_scheduler_id = 0;
unsigned int scheduler::m_coro_scheduler_count           = 0;

scheduler::scheduler() {
  m_id = ++m_coro_scheduler_count;
  m_thread_cxts.reserve(128);
  thread_context *io_cxt = new thread_context;
  io_cxt->m_tasks        = new task_queue(64);
  m_thread_cxts.push_back(io_cxt);
  spawn_workers(std::thread::hardware_concurrency());
}

scheduler::~scheduler() {
  m_stop_requested = true;
  m_task_wait_flag.test_and_set(std::memory_order_relaxed);
  m_task_wait_flag.notify_all();
  for (auto &t_cxt : this->m_thread_cxts) {
    if (t_cxt->m_thread.joinable())
      t_cxt->m_thread.join();
  }
}

bool scheduler::steal_task(std::coroutine_handle<> &handle) noexcept {
  auto total_threads = m_total_threads.load(std::memory_order_relaxed);

  bool c = false;
  do {
    c              = false;
    unsigned int i = (m_thread_id + 1) % (total_threads + 1);
    do {
      if (m_thread_cxts[i]->m_tasks->steal(handle)) {
        return true;
      }
      c = c || !m_thread_cxts[i]->m_tasks->empty();
      i = (i + 1) % (total_threads + 1);
    } while (i != m_thread_id);
  } while (c);

  return false;
}

void scheduler::schedule(const std::coroutine_handle<> &handle) noexcept {
  if (m_coro_scheduler_id != m_id || m_thread_id == 0) [[unlikely]] {
    std::unique_lock lk(m_global_task_queue_mutex);
    m_thread_cxts[0]->m_tasks->enqueue(handle);
  } else [[likely]] {
    m_thread_cxts[m_thread_id]->m_tasks->enqueue(handle);
  }
  m_task_wait_flag.test_and_set(std::memory_order_relaxed);
  m_task_wait_flag.notify_one();
}

bool scheduler::peek_next_coroutine(std::coroutine_handle<> &handle) noexcept {
  return m_thread_cxts[m_thread_id]->m_tasks->dequeue(handle)
             ? true
             : steal_task(handle);
}

std::coroutine_handle<> scheduler::get_waiting_channel() noexcept {
  return m_thread_cxts[m_thread_id]->m_waiting_channel;
}

auto scheduler::get_next_coroutine() noexcept -> std::coroutine_handle<> {
  std::coroutine_handle<> handle;
  return peek_next_coroutine(handle) ? handle : get_waiting_channel();
}

scheduler_task scheduler::awaiter() {
  struct thread_awaiter {
    std::coroutine_handle<> &m_handle_ref;

    constexpr bool await_ready() const noexcept { return false; }

    auto await_suspend(const std::coroutine_handle<> &) const noexcept {
      return m_handle_ref;
    }

    constexpr void await_resume() const noexcept {}
  };

  while (!m_stop_requested) {
    std::coroutine_handle<> handle;

    std::unique_lock<std::mutex> lk(m_task_mutex);
    while (!peek_next_coroutine(handle)) {
      m_task_wait_flag.wait(false, std::memory_order_relaxed);
      if (m_stop_requested) [[unlikely]] {
        co_return;
      }
      m_task_wait_flag.clear(std::memory_order_relaxed);
    }
    lk.unlock();

    co_await thread_awaiter{handle};
  }
}

void scheduler::spawn_workers(const unsigned int &count) {
  std::unique_lock<std::mutex> lk(m_spawn_thread_mutex);
  for (unsigned int i = 0; i < count; ++i) {
    init_thread();
    m_thread_cxts[m_total_threads]->m_thread = std::thread(
        [&](unsigned int id) {
          m_thread_id         = id;
          m_coro_scheduler_id = m_id;
          m_thread_cxts[id]->m_waiting_channel.resume();
        },
        ++m_total_threads);
  }
}

void scheduler::init_thread() {
  thread_context *cxt           = new thread_context;
  cxt->m_thread_status.m_status = thread_status::STATUS::READY;
  cxt->m_tasks                  = new task_queue(64);
  cxt->m_waiting_channel        = awaiter().handle();
  m_thread_cxts.push_back(cxt);
}

void scheduler::set_thread_status(thread_status::STATUS status) noexcept {

  switch (status) {
  case thread_status::STATUS::READY:
    set_thread_ready();
    break;
  case thread_status::STATUS::RUNNING:
    set_thread_running();
    break;
  case thread_status::STATUS::SUSPENDED:
    set_thread_suspended();
    break;
  default:
    break;
  }
}

void scheduler::set_thread_suspended() noexcept {
  this->m_total_running_threads.fetch_sub(1, std::memory_order_relaxed);
  if ((this->m_total_suspended_threads.fetch_add(1, std::memory_order_relaxed) +
       1) >= m_total_threads.load(std::memory_order_relaxed)) {
    spawn_workers(1);
  }
  m_thread_cxts[m_thread_id]->m_thread_status.m_status.store(
      thread_status::STATUS::SUSPENDED, std::memory_order_relaxed);
}

void scheduler::set_thread_ready() noexcept {
  this->m_total_running_threads.fetch_sub(1, std::memory_order_relaxed);
  this->m_total_ready_threads.fetch_add(1, std::memory_order_relaxed);
  m_thread_cxts[m_thread_id]->m_thread_status.m_status.store(
      thread_status::STATUS::READY, std::memory_order_relaxed);
}

void scheduler::set_thread_running() noexcept {
  if (m_thread_cxts[m_thread_id]->m_thread_status.m_status.load(
          std::memory_order_relaxed) == thread_status::STATUS::SUSPENDED) {
    this->m_total_suspended_threads.fetch_sub(1, std::memory_order_relaxed);
  } else {
    this->m_total_ready_threads.fetch_sub(1, std::memory_order_relaxed);
  }
  this->m_total_running_threads.fetch_add(1, std::memory_order_relaxed);
  m_thread_cxts[m_thread_id]->m_thread_status.m_status.store(
      thread_status::STATUS::RUNNING, std::memory_order_relaxed);
}
