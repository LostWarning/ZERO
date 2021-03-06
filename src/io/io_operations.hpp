#ifndef __IO_IO_OPERATION_HPP__
#define __IO_IO_OPERATION_HPP__

#include "io_uring_op.hpp"

#include <sys/timerfd.h>
#include <vector>

template <typename IO_SERVICE>
class io_operation {
  IO_SERVICE *m_io_service;

public:
  explicit io_operation(IO_SERVICE *service) : m_io_service{service} {}

  auto nop(unsigned char sqe_flags = 0) -> uring_awaiter {
    return m_io_service->submit_io(io_uring_op_nop_t(sqe_flags));
  }

  auto delay(const unsigned long &sec, const unsigned long &nsec,
             unsigned char sqe_flags = 0) {

    int tfd = timerfd_create(CLOCK_REALTIME, 0);
    itimerspec spec;
    spec.it_value.tv_sec     = sec;
    spec.it_value.tv_nsec    = nsec;
    spec.it_interval.tv_sec  = 0;
    spec.it_interval.tv_nsec = 0;
    timerfd_settime(tfd, 0, &spec, NULL);

    poll_add(tfd, POLL_IN, sqe_flags | IOSQE_IO_HARDLINK);
    return close(tfd, sqe_flags);
  }

  auto poll_add(const int &fd, const unsigned &poll_mask,
                unsigned char sqe_flags = 0) -> uring_awaiter {
    return m_io_service->submit_io(
        io_uring_op_poll_add_t(fd, poll_mask, sqe_flags));
  }

  auto cancel(uring_awaiter &awaiter, const int &flags,
              unsigned char sqe_flags = 0) -> uring_awaiter {
    return m_io_service->submit_io(
        io_uring_op_cancel_t(awaiter.get_data(), flags, sqe_flags));
  }

  auto openat(const int &dfd, const char *const &filename, const int &flags,
              const mode_t &mode, unsigned char sqe_flags = 0)
      -> uring_awaiter {
    return m_io_service->submit_io(
        io_uring_op_openat_t(dfd, filename, flags, mode, sqe_flags));
  }

  auto read(const int &fd, void *const &buffer, const unsigned &bytes,
            const off_t &offset, unsigned char sqe_flags = 0) -> uring_awaiter {
    return m_io_service->submit_io(
        io_uring_op_read_t(fd, buffer, bytes, offset, sqe_flags));
  }

  auto read(const int &fd, const int &gbid, const unsigned &bytes,
            const off_t &offset, unsigned char sqe_flags = 0) -> uring_awaiter {
    return m_io_service->submit_io(
        io_uring_op_read_provide_buffer_t(fd, gbid, bytes, offset, sqe_flags));
  }

  auto readv(const int &fd, iovec *const &iovecs, const unsigned int &count,
             const off_t &offset, unsigned char &sqe_flags = 0)
      -> uring_awaiter {
    return m_io_service->submit_io(
        io_uring_op_readv_t(fd, iovecs, count, offset, sqe_flags));
  }

  auto read_fixed(const int &fd, void *const &buffer, const unsigned &bytes,
                  const off_t &offset, const int &buf_index,
                  unsigned char sqe_flags = 0) -> uring_awaiter {
    return m_io_service->submit_io(io_uring_op_read_fixed_t(
        fd, buffer, bytes, offset, buf_index, sqe_flags));
  }

  auto write(const int &fd, void *const &buffer, const unsigned &bytes,
             const off_t &offset, unsigned char sqe_flags = 0)
      -> uring_awaiter {
    return m_io_service->submit_io(
        io_uring_op_write_t(fd, buffer, bytes, offset, sqe_flags));
  }

  auto writev(const int &fd, iovec *const &iovecs, const unsigned int &count,
              const off_t &offset, unsigned char &sqe_flags = 0)
      -> uring_awaiter {
    return m_io_service->submit_io(
        io_uring_op_writev_t(fd, iovecs, count, offset, sqe_flags));
  }

  auto write_fixed(const int &fd, void *const &buffer, const unsigned &bytes,
                   const off_t &offset, const int &buf_index,
                   unsigned char sqe_flags = 0) {
    return m_io_service->submit_io(io_uring_op_write_fixed_t(
        fd, buffer, bytes, offset, buf_index, sqe_flags));
  }

  auto recv(const int &fd, void *const &buffer, const size_t &length,
            const int &flags, unsigned char sqe_flags = 0) -> uring_awaiter {
    return m_io_service->submit_io(
        io_uring_op_recv_t(fd, buffer, length, flags, sqe_flags));
  }

  auto recv(const int &fd, const int &gbid, const size_t &length,
            const int &flags, unsigned char sqe_flags = 0) -> uring_awaiter {
    return m_io_service->submit_io(
        io_uring_op_recv_provide_buffer_t(fd, gbid, length, flags, sqe_flags));
  }

  auto accept(const int &fd, sockaddr *const &client_info,
              socklen_t *const &socklen, const int &flags,
              unsigned char sqe_flags = 0) -> uring_awaiter {
    return m_io_service->submit_io(
        io_uring_op_accept_t(fd, client_info, socklen, flags, sqe_flags));
  }

  auto send(const int &fd, void *const &buffer, const size_t &length,
            const int &flags, unsigned char sqe_flags = 0) -> uring_awaiter {
    return m_io_service->submit_io(
        io_uring_op_send_t(fd, buffer, length, flags, sqe_flags));
  }

  auto close(const int &fd, unsigned char sqe_flags = 0) -> uring_awaiter {
    return m_io_service->submit_io(io_uring_op_close_t(fd, sqe_flags));
  }

  auto statx(int dfd, const char *path, int flags, unsigned mask,
             struct statx *statxbuf, unsigned char sqe_flags = 0) {
    return m_io_service->submit_io(
        io_uring_op_statx_t(dfd, path, flags, mask, statxbuf, sqe_flags));
  }

  auto timeout(__kernel_timespec *const &t, unsigned char sqe_flags = 0)
      -> uring_awaiter {
    return m_io_service->submit_io(io_uring_op_timeout_t(t, sqe_flags));
  }

  auto link_timeout(__kernel_timespec *const &t, const unsigned &flags,
                    unsigned char sqe_flags = 0) -> uring_awaiter {
    return m_io_service->submit_io(
        io_uring_op_link_timeout_t(t, flags, sqe_flags));
  }

  auto provide_buffer(void *const addr, int buffer_length, int buffer_count,
                      int bgid, int bid = 0, unsigned char sqe_flags = 0) {
    return m_io_service->submit_io(io_uring_op_provide_buffer_t(
        addr, buffer_length, buffer_count, bgid, bid, sqe_flags));
  }
};

enum class IO_OP_TYPE { BATCH, LINK };

template <typename IO_Service, IO_OP_TYPE Type>
class io_operation_detached
    : public io_operation<io_operation_detached<IO_Service, Type>> {
  IO_Service *m_io_service;
  std::vector<io_uring_op> m_io_operations;

public:
  explicit io_operation_detached(IO_Service *io_service)
      : io_operation<io_operation_detached<IO_Service, Type>>(this)
      , m_io_service{io_service} {}

  std::vector<io_uring_op> &operations() { return m_io_operations; }

  template <IO_URING_OP OP>
  auto submit_io(OP &&operation) -> uring_awaiter {
    auto future = operation.get_future(m_io_service->get_awaiter_allocator());
    m_io_operations.push_back(std::forward<OP>(operation));
    return future;
  }
};

template <typename IO_Service>
using io_batch = io_operation_detached<IO_Service, IO_OP_TYPE::BATCH>;

template <typename IO_Service>
using io_link = io_operation_detached<IO_Service, IO_OP_TYPE::LINK>;

#endif
