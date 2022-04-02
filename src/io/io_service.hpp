#ifndef __IO_IO_SERVICE_HPP__
#define __IO_IO_SERVICE_HPP__

#include "queue/pool_allocator.hpp"
#include "queue/queue.hpp"

#include "io_pipeline.hpp"

#include <atomic>
#include <condition_variable>
#include <coroutine>
#include <iostream>
#include <liburing.h>
#include <liburing/io_uring.h>
#include <memory>

class io_service {
  static thread_local unsigned int m_thread_id;
  std::vector<io_op_pipeline *> m_io_queues;
  std::vector<pool_allocator<uring_data, 128> *> m_uring_data_allocators;

  io_uring m_uring;

  unsigned int m_entries;
  unsigned int m_flags;

  std::atomic_bool m_io_sq_running{false};
  std::thread m_io_cq_thread;
  std::atomic_bool m_queue_runner{false};
  std::atomic_int m_io_queue_count{0};

protected:
  unsigned int drain_io_results() {
    unsigned completed = 0;
    unsigned head;
    struct io_uring_cqe *cqe;

    io_uring_for_each_cqe(&m_uring, head, cqe) {
      ++completed;
      auto data = static_cast<uring_data *>(io_uring_cqe_get_data(cqe));
      if (data != nullptr) {
        data->m_result = cqe->res;
        data->m_flags  = cqe->flags;
        if (data->m_handle_ctl.exchange(true, std::memory_order_acq_rel)) {
          data->m_scheduler->schedule(data->m_handle);
        }
      }
    }
    if (completed) {
      io_uring_cq_advance(&m_uring, completed);
    }
    return completed;
  }

  void io_loop() noexcept {
    while (true) {
      io_uring_cqe *cqe = nullptr;
      if (io_uring_wait_cqe(&m_uring, &cqe) == 0) {
        auto data = static_cast<uring_data *>(io_uring_cqe_get_data(cqe));
        if (data != nullptr) {
          data->m_result = cqe->res;
          data->m_flags  = cqe->flags;
          if (data->m_handle_ctl.exchange(true, std::memory_order_acq_rel)) {
            data->m_scheduler->schedule(data->m_handle);
          }
        }
        io_uring_cqe_seen(&m_uring, cqe);
      } else {
        std::cerr << "Wait CQE Failed\n";
      }

      drain_io_results();
    }
    return;
  }

public:
  class awaiter {
    uring_data *m_data;
    io_service *m_service;

  public:
    awaiter(const awaiter &) = delete;

    awaiter(awaiter &&Other) {
      m_data    = Other.m_data;
      m_service = Other.m_service;

      Other.m_data    = nullptr;
      Other.m_service = nullptr;
    }

    bool await_ready() const noexcept {
      return m_data->m_handle_ctl.load(std::memory_order_relaxed);
    }

    auto await_suspend(const std::coroutine_handle<> &handle) noexcept
        -> std::coroutine_handle<> {
      m_data->m_handle = handle;
      auto schd        = m_data->m_scheduler;
      if (m_data->m_handle_ctl.exchange(true, std::memory_order_acq_rel)) {
        return handle;
      }
      return schd->get_next_coroutine();
    }
    int await_resume() const noexcept {
      // Acquire the changes to the result
      m_data->m_handle_ctl.load(std::memory_order_acq_rel);
      return m_data->m_result;
    }

    awaiter(io_service *service) : m_service{service} {
      m_data = m_service->m_uring_data_allocators[m_service->m_thread_id]
                   ->allocate();
    }

    ~awaiter() {
      if (m_service != nullptr) {
        m_service->m_uring_data_allocators[m_service->m_thread_id]->deallocate(
            m_data);
      }
    }

    uring_data *get_data() const noexcept { return m_data; }

    void via(scheduler *s) { this->m_data->m_scheduler = s; }
  };

  io_service(const u_int &entries, const u_int &flags)
      : m_entries(entries), m_flags(flags) {
    io_uring_queue_init(entries, &m_uring, flags);
    m_io_cq_thread = std::move(std::thread([&] { this->io_loop(); }));

    m_io_queues.resize(32);
    for (size_t i = 0; i < 32; ++i) {
      m_io_queues[i] = new io_op_pipeline(128);
    }

    m_uring_data_allocators.resize(32);
    for (size_t i = 0; i < 32; ++i) {
      m_uring_data_allocators[i] = new pool_allocator<uring_data, 128>();
    }
  }

  template <typename Operation>
  auto submit_io(Operation &&operation) -> awaiter {

    if (m_thread_id == 0) {
      m_thread_id =
          m_io_queue_count.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    awaiter future(this);
    operation.m_data = future.get_data();

    m_io_queues[m_thread_id - 1]->enqueue(operation);

    auto is_queues_empty = [&] {
      bool is_empty = true;
      for (auto &q : this->m_io_queues) {
        is_empty = is_empty && q->empty();
      }
      return is_empty;
    };

    while (!is_queues_empty() &&
           !m_io_sq_running.exchange(true, std::memory_order_relaxed)) {

      while (!is_queues_empty()) {
        for (auto &q : m_io_queues) {
          q->init_io_uring_ops(&m_uring);
          io_uring_submit(&m_uring);
        }
      }

      m_io_sq_running.store(false, std::memory_order_relaxed);
    }

    return future;
  }

  auto openat(const int &dfd, const char *const &filename, const int &flags,
              const mode_t &mode) -> awaiter {
    return submit_io(io_uring_op_openat_t{dfd, filename, flags, mode, nullptr});
  }

  auto read(const int &fd, void *const &buffer, const unsigned &bytes,
            const off_t &offset) -> awaiter {
    return submit_io(io_uring_op_read_t{fd, buffer, bytes, offset, nullptr});
  }

  auto write(const int &fd, void *const &buffer, const unsigned &bytes,
             const off_t &offset) -> awaiter {
    return submit_io(io_uring_op_write_t{fd, buffer, bytes, offset, nullptr});
  }

  auto recv(const int &m_fd, void *const &m_buffer, const size_t &m_length,
            const int &m_flags) -> awaiter {
    return submit_io(
        io_uring_op_recv_t{m_fd, m_buffer, m_length, m_flags, nullptr});
  }

  auto accept(const int &fd, sockaddr *const &client_info,
              socklen_t *const &socklen, const int &flags) -> awaiter {
    return submit_io(
        io_uring_op_accept_t{fd, client_info, socklen, flags, nullptr});
  }

  auto send(const int &fd, void *const &buffer, const size_t &length,
            const int &flags) -> awaiter {
    return submit_io(io_uring_op_send_t{fd, buffer, length, flags, nullptr});
  }

  auto close(const int &fd) -> awaiter {
    return submit_io(io_uring_op_close_t{fd, nullptr});
  }

  auto sleep(__kernel_timespec *const &t) -> awaiter {
    return submit_io(io_uring_op_sleep_t{t, nullptr});
  }
};

#endif