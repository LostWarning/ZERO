#ifndef __IO_IO_LINK_HPP__
#define __IO_IO_LINK_HPP__

#include "io_operations.hpp"

#include <vector>

template <typename IO_SERVICE>
class io_link {
  IO_SERVICE *m_io_service;
  std::vector<io_operation> m_io_operations;

public:
  explicit io_link(IO_SERVICE *service) : m_io_service{service} {}

  io_link(const io_link &) = delete;
  io_link &operator=(const io_link &) = delete;

  io_link(io_link &&Other) {
    this->m_io_service    = Other.m_io_operations;
    this->m_io_operations = std::move(Other.m_io_operations);

    Other.m_io_service = nullptr;
  }

  io_link &operator=(io_link &&Other) {
    this->m_io_service    = Other.m_io_operations;
    this->m_io_operations = std::move(Other.m_io_operations);

    Other.m_io_service = nullptr;
  }

  auto openat(const int &dfd, const char *const &filename, const int &flags,
              const mode_t &mode, unsigned char sqe_flags = 0)
      -> uring_awaiter {
    return link(io_uring_op_openat_t(dfd, filename, flags, mode,
                                     sqe_flags | IOSQE_IO_HARDLINK));
  }

  auto read(const int &fd, void *const &buffer, const unsigned &bytes,
            const off_t &offset, unsigned char sqe_flags = 0) -> uring_awaiter {
    return link(io_uring_op_read_t(fd, buffer, bytes, offset,
                                   sqe_flags | IOSQE_IO_HARDLINK));
  }

  auto write(const int &fd, void *const &buffer, const unsigned &bytes,
             const off_t &offset, unsigned char sqe_flags = 0)
      -> uring_awaiter {
    return link(io_uring_op_write_t(fd, buffer, bytes, offset,
                                    sqe_flags | IOSQE_IO_HARDLINK));
  }

  auto recv(const int &fd, void *const &buffer, const size_t &length,
            const int &flags, unsigned char sqe_flags = 0) -> uring_awaiter {
    return link(io_uring_op_recv_t(fd, buffer, length, flags,
                                   sqe_flags | IOSQE_IO_HARDLINK));
  }

  auto accept(const int &fd, sockaddr *const &client_info,
              socklen_t *const &socklen, const int &flags,
              unsigned char sqe_flags = 0) -> uring_awaiter {
    return link(io_uring_op_accept_t(fd, client_info, socklen, flags,
                                     sqe_flags | IOSQE_IO_HARDLINK));
  }

  auto send(const int &fd, void *const &buffer, const size_t &length,
            const int &flags, unsigned char sqe_flags = 0) -> uring_awaiter {
    return link(io_uring_op_send_t(fd, buffer, length, flags,
                                   sqe_flags | IOSQE_IO_HARDLINK));
  }

  auto close(const int &fd, unsigned char sqe_flags = 0) -> uring_awaiter {
    return link(io_uring_op_close_t(fd, sqe_flags | IOSQE_IO_HARDLINK));
  }

  auto sleep(__kernel_timespec *const &t, unsigned char sqe_flags = 0)
      -> uring_awaiter {
    return link(io_uring_op_sleep_t(t, sqe_flags | IOSQE_IO_HARDLINK));
  }

  std::vector<io_operation> &operations() { return m_io_operations; }

protected:
  template <IO_URING_OP OP>
  auto link(OP &&operation) -> uring_awaiter {
    auto future = operation.get_future(m_io_service->get_awaiter_allocator());
    m_io_operations.push_back(std::forward<OP>(operation));
    return future;
  }
};

#endif