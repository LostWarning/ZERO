#ifndef __IO_IO_OPERATION_HPP__
#define __IO_IO_OPERATION_HPP__

#include "io_uring_op.hpp"

#include <vector>

enum class OP_TYPE { BATCH, LINK };

template <typename IO_SERVICE, OP_TYPE TYPE>
class io_operation {
  IO_SERVICE *m_io_service;
  std::vector<io_uring_op> m_io_operations;

public:
  explicit io_operation(IO_SERVICE *service) : m_io_service{service} {}

  io_operation(const io_operation &) = delete;
  io_operation &operator=(const io_operation &) = delete;

  io_operation(io_operation &&Other) {
    this->m_io_service    = Other.m_io_operations;
    this->m_io_operations = std::move(Other.m_io_operations);

    Other.m_io_service = nullptr;
  }

  io_operation &operator=(io_operation &&Other) {
    this->m_io_service    = Other.m_io_operations;
    this->m_io_operations = std::move(Other.m_io_operations);

    Other.m_io_service = nullptr;
  }

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

  std::vector<io_uring_op> &operations() { return m_io_operations; }

protected:
  template <IO_URING_OP OP>
  auto submit_io(OP &&operation) -> uring_awaiter {
    auto future = operation.get_future(m_io_service->get_awaiter_allocator());
    m_io_operations.push_back(std::forward<OP>(operation));
    return future;
  }
};

#endif
