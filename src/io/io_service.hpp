#ifndef __IO_IO_SERVICE_HPP__
#define __IO_IO_SERVICE_HPP__

#include "io_batch.hpp"
#include "io_link.hpp"
#include "io_pipeline.hpp"
#include "uring_data.hpp"

#include <atomic>
#include <condition_variable>
#include <coroutine>
#include <iostream>
#include <liburing.h>
#include <liburing/io_uring.h>
#include <memory>

// TODO: if io_uring_submit fails then the submission should start from the last
// submitted queue to keep order of the linked requests

class io_service {
  static thread_local unsigned int m_thread_id;
  static thread_local uring_data::allocator *m_uio_data_allocator;

  std::vector<io_op_pipeline *> m_io_queues;
  std::vector<uring_data::allocator *> m_uio_data_allocators;

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

  auto batch() { return io_batch(this); }

  auto link() { return io_link(this); }

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

  uring_data::allocator *get_awaiter_allocator() {
    setup_uring_allocator();
    return m_uio_data_allocator;
  }

  void submit(io_batch<io_service> &batch);

  void submit(io_link<io_service> &link);

protected:
  template <IO_URING_OP OP>
  auto submit_io(OP &&operation) -> uring_awaiter {

    setup_uring_allocator();

    auto future = operation.get_future(m_uio_data_allocator);

    m_io_queues[m_thread_id - 1]->enqueue(std::forward<OP>(operation));

    submit();

    return future;
  }

  unsigned int drain_io_results();

  void io_loop() noexcept;

  bool io_queue_empty() const noexcept;

  void setup_uring_allocator();

  void handle_completion(io_uring_cqe *cqe);
};

#endif