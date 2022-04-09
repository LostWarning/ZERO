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

  template <size_t n>
  bool register_buffer(iovec (&io_vec)[n]) {
    io_uring_register_buffers(&m_uring, io_vec, n);
    return true;
  }

  auto batch() { return io_batch(this); }

  auto link() { return io_link(this); }

  auto openat(const int &dfd, const char *const &filename, const int &flags,
              const mode_t &mode, unsigned char sqe_flags = 0)
      -> uring_awaiter {
    return submit_io(
        io_uring_op_openat_t(dfd, filename, flags, mode, sqe_flags));
  }

  auto read(const int &fd, void *const &buffer, const unsigned &bytes,
            const off_t &offset, unsigned char sqe_flags = 0) -> uring_awaiter {
    return submit_io(io_uring_op_read_t(fd, buffer, bytes, offset, sqe_flags));
  }

  auto read_fixed(const int &fd, void *const &buffer, const unsigned &bytes,
                  const off_t &offset, const int &buf_index,
                  unsigned char sqe_flags = 0) -> uring_awaiter {
    return submit_io(io_uring_op_read_fixed_t(fd, buffer, bytes, offset,
                                              buf_index, sqe_flags));
  }

  auto write(const int &fd, void *const &buffer, const unsigned &bytes,
             const off_t &offset, unsigned char sqe_flags = 0)
      -> uring_awaiter {
    return submit_io(io_uring_op_write_t(fd, buffer, bytes, offset, sqe_flags));
  }

  auto write_fixed(const int &fd, void *const &buffer, const unsigned &bytes,
                   const off_t &offset, const int &buf_index,
                   unsigned char sqe_flags = 0) {
    return submit_io(io_uring_op_write_fixed_t(fd, buffer, bytes, offset,
                                               buf_index, sqe_flags));
  }

  auto recv(const int &fd, void *const &buffer, const size_t &length,
            const int &flags, unsigned char sqe_flags = 0) -> uring_awaiter {
    return submit_io(io_uring_op_recv_t(fd, buffer, length, flags, sqe_flags));
  }

  auto accept(const int &fd, sockaddr *const &client_info,
              socklen_t *const &socklen, const int &flags,
              unsigned char sqe_flags = 0) -> uring_awaiter {
    return submit_io(
        io_uring_op_accept_t(fd, client_info, socklen, flags, sqe_flags));
  }

  auto send(const int &fd, void *const &buffer, const size_t &length,
            const int &flags, unsigned char sqe_flags = 0) -> uring_awaiter {
    return submit_io(io_uring_op_send_t(fd, buffer, length, flags, sqe_flags));
  }

  auto close(const int &fd, unsigned char sqe_flags = 0) -> uring_awaiter {
    return submit_io(io_uring_op_close_t(fd, sqe_flags));
  }

  auto sleep(__kernel_timespec *const &t, unsigned char sqe_flags = 0)
      -> uring_awaiter {
    return submit_io(io_uring_op_sleep_t(t, sqe_flags));
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