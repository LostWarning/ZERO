#ifndef __IO_IO_SERVICE_HPP__
#define __IO_IO_SERVICE_HPP__

#include "queue/queue.hpp"
#include "uring_data.hpp"

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
  static thread_local uring_allocator *m_uring_data_allocator;

  std::vector<io_op_pipeline *> m_io_queues;
  std::vector<uring_allocator *> m_uring_data_allocators;

  io_uring m_uring;

  unsigned int m_entries;
  unsigned int m_flags;

  std::thread m_io_cq_thread;
  std::atomic_bool m_io_sq_running{false};
  std::atomic_int m_io_queue_count{0};

public:
  io_service(const u_int &entries, const u_int &flags)
      : m_entries(entries), m_flags(flags) {
    io_uring_queue_init(entries, &m_uring, flags);
    m_io_cq_thread = std::move(std::thread([&] { this->io_loop(); }));

    m_io_queues.resize(32);
    for (size_t i = 0; i < 32; ++i) {
      m_io_queues[i] = new io_op_pipeline(128);
    }
  }

  template <typename Operation>
  auto submit_io(Operation &&operation) -> uring_awaiter {

    if (m_thread_id == 0) {
      m_thread_id =
          m_io_queue_count.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    if (m_uring_data_allocator == nullptr) {
      m_uring_data_allocator = new uring_allocator;
      m_uring_data_allocators.push_back(m_uring_data_allocator);
    }

    uring_awaiter future(m_uring_data_allocator);
    operation.m_data = future.get_data();

    m_io_queues[m_thread_id - 1]->enqueue(operation);

    while (!io_queue_empty() &&
           !m_io_sq_running.exchange(true, std::memory_order_seq_cst)) {

      while (!io_queue_empty()) {
        for (auto &q : m_io_queues) {
          if (q->init_io_uring_ops(&m_uring)) {
            io_uring_submit(&m_uring);
          }
        }
      }

      m_io_sq_running.store(false, std::memory_order_relaxed);
    }

    return future;
  }

  auto openat(const int &dfd, const char *const &filename, const int &flags,
              const mode_t &mode) -> uring_awaiter {
    return submit_io(io_uring_op_openat_t{dfd, filename, flags, mode, nullptr});
  }

  auto read(const int &fd, void *const &buffer, const unsigned &bytes,
            const off_t &offset) -> uring_awaiter {
    return submit_io(io_uring_op_read_t{fd, buffer, bytes, offset, nullptr});
  }

  auto write(const int &fd, void *const &buffer, const unsigned &bytes,
             const off_t &offset) -> uring_awaiter {
    return submit_io(io_uring_op_write_t{fd, buffer, bytes, offset, nullptr});
  }

  auto recv(const int &m_fd, void *const &m_buffer, const size_t &m_length,
            const int &m_flags) -> uring_awaiter {
    return submit_io(
        io_uring_op_recv_t{m_fd, m_buffer, m_length, m_flags, nullptr});
  }

  auto accept(const int &fd, sockaddr *const &client_info,
              socklen_t *const &socklen, const int &flags) -> uring_awaiter {
    return submit_io(
        io_uring_op_accept_t{fd, client_info, socklen, flags, nullptr});
  }

  auto send(const int &fd, void *const &buffer, const size_t &length,
            const int &flags) -> uring_awaiter {
    return submit_io(io_uring_op_send_t{fd, buffer, length, flags, nullptr});
  }

  auto close(const int &fd) -> uring_awaiter {
    return submit_io(io_uring_op_close_t{fd, nullptr});
  }

  auto sleep(__kernel_timespec *const &t) -> uring_awaiter {
    return submit_io(io_uring_op_sleep_t{t, nullptr});
  }

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

  bool io_queue_empty() {
    bool is_empty = true;
    for (auto &q : this->m_io_queues) {
      is_empty = is_empty && q->empty();
    }
    return is_empty;
  }
};

#endif