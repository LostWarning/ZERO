#ifndef __IO_IO_SERVICE_HPP__
#define __IO_IO_SERVICE_HPP__

#include "uring_data.hpp"

#include "io_pipeline.hpp"

#include <atomic>
#include <condition_variable>
#include <coroutine>
#include <iostream>
#include <liburing.h>
#include <liburing/io_uring.h>
#include <memory>
#include <span>

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
  io_service(const u_int &entries, const u_int &flags);

  ~io_service();

  template <typename T, size_t n>
  bool create_fixed_buffer(T (&size)[n]) {
    iovec io_vec[n];
    for (size_t i = 0; i < n; ++i) {
      io_vec[i].iov_base = new char[size[i]];
      io_vec[i].iov_len  = size[i];
    }

    io_uring_register_buffers(&m_uring, io_vec, n);
    return true;
  }

  auto openat(const int &dfd, const char *const &filename, const int &flags,
              const mode_t &mode) -> uring_awaiter {
    return submit_io(io_uring_op_openat_t(dfd, filename, flags, mode));
  }

  auto read(const int &fd, void *const &buffer, const unsigned &bytes,
            const off_t &offset) -> uring_awaiter {
    return submit_io(io_uring_op_read_t(fd, buffer, bytes, offset));
  }

  auto write(const int &fd, void *const &buffer, const unsigned &bytes,
             const off_t &offset) -> uring_awaiter {
    return submit_io(io_uring_op_write_t(fd, buffer, bytes, offset));
  }

  auto recv(const int &fd, void *const &buffer, const size_t &length,
            const int &flags) -> uring_awaiter {
    return submit_io(io_uring_op_recv_t(fd, buffer, length, flags));
  }

  auto accept(const int &fd, sockaddr *const &client_info,
              socklen_t *const &socklen, const int &flags) -> uring_awaiter {
    return submit_io(io_uring_op_accept_t(fd, client_info, socklen, flags));
  }

  auto send(const int &fd, void *const &buffer, const size_t &length,
            const int &flags) -> uring_awaiter {
    return submit_io(io_uring_op_send_t(fd, buffer, length, flags));
  }

  auto close(const int &fd) -> uring_awaiter {
    return submit_io(io_uring_op_close_t(fd));
  }

  auto sleep(__kernel_timespec *const &t) -> uring_awaiter {
    return submit_io(io_uring_op_sleep_t(t));
  }

  void submit();

protected:
  auto submit_io(IO_URING_OP auto &&operation) -> uring_awaiter {

    setup_uring_allocator();

    auto future = operation.get_future(m_uring_data_allocator);

    m_io_queues[m_thread_id - 1]->enqueue(operation);

    submit();

    return future;
  }

  unsigned int drain_io_results();

  void io_loop() noexcept;

  bool io_queue_empty() const noexcept;

  void setup_uring_allocator();
};

#endif