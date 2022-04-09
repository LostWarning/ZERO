#ifndef __IO_IO_OPERATIONS_HPP__
#define __IO_IO_OPERATIONS_HPP__

#include "uring_data.hpp"

#include <concepts>
#include <variant>

template <typename T>
concept IO_URING_OP = requires(T a, io_uring *const uring,
                               uring_data::allocator *alloc) {
  { a.run(uring) } -> std::same_as<bool>;
  { a.get_future(alloc) } -> std::same_as<uring_awaiter>;
};

class io_uring_future {
  uring_data *m_data;

public:
  uring_awaiter get_future(uring_data::allocator *allocator) {
    uring_awaiter awaiter(allocator);
    m_data = awaiter.get_data();
    return awaiter;
  }

protected:
  void set_data(io_uring_sqe *sqe) { io_uring_sqe_set_data(sqe, m_data); }
};

class io_uring_op_openat_t : public io_uring_future {
  int m_dir;
  const char *m_filename;
  int m_flags;
  mode_t m_mode;

public:
  io_uring_op_openat_t() = default;

  io_uring_op_openat_t(const int &dir, const char *const &filename,
                       const int &flags, const mode_t &mode)
      : m_dir{dir}, m_filename{filename}, m_flags{flags}, m_mode{mode} {}

  bool run(io_uring *const uring) {
    io_uring_sqe *sqe;
    if ((sqe = io_uring_get_sqe(uring)) == nullptr) {
      return false;
    }
    io_uring_prep_openat(sqe, m_dir, m_filename, m_flags, m_mode);
    set_data(sqe);
    return true;
  }
};

class io_uring_op_read_t : public io_uring_future {
  int m_fd;
  void *m_buffer;
  unsigned m_bytes;
  off_t m_offset;

public:
  io_uring_op_read_t() = default;

  io_uring_op_read_t(const int &fd, void *const &buffer, const unsigned &bytes,
                     const off_t &offset)
      : m_fd{fd}, m_buffer{buffer}, m_bytes{bytes}, m_offset{offset} {}

  bool run(io_uring *const uring) {
    io_uring_sqe *sqe;
    if ((sqe = io_uring_get_sqe(uring)) == nullptr) {
      return false;
    }
    io_uring_prep_read(sqe, m_fd, m_buffer, m_bytes, m_offset);
    set_data(sqe);
    return true;
  }
};

class io_uring_op_write_t : public io_uring_future {
  int m_fd;
  void *m_buffer;
  unsigned m_bytes;
  off_t m_offset;

public:
  io_uring_op_write_t() = default;

  io_uring_op_write_t(const int &fd, void *const &buffer, const unsigned &bytes,
                      const off_t &offset)
      : m_fd{fd}, m_buffer{buffer}, m_bytes{bytes}, m_offset{offset} {}

  bool run(io_uring *const uring) {
    io_uring_sqe *sqe;
    if ((sqe = io_uring_get_sqe(uring)) == nullptr) {
      return false;
    }
    io_uring_prep_write(sqe, m_fd, m_buffer, m_bytes, m_offset);
    set_data(sqe);
    return true;
  }
};

class io_uring_op_sleep_t : public io_uring_future {
  __kernel_timespec *m_time;

public:
  io_uring_op_sleep_t() = default;

  io_uring_op_sleep_t(__kernel_timespec *const &t) : m_time{t} {}

  bool run(io_uring *const uring) {
    io_uring_sqe *sqe;
    if ((sqe = io_uring_get_sqe(uring)) == nullptr) {
      return false;
    }
    io_uring_prep_timeout(sqe, m_time, 0, 0);
    set_data(sqe);
    return true;
  }
};

class io_uring_op_recv_t : public io_uring_future {
  int m_fd;
  void *m_buffer;
  size_t m_length;
  int m_flags;

public:
  io_uring_op_recv_t() = default;

  io_uring_op_recv_t(const int &fd, void *const &buffer, const size_t &length,
                     const int &flags)
      : m_fd{fd}, m_buffer{buffer}, m_length{length}, m_flags{flags} {}

  bool run(io_uring *const uring) {
    io_uring_sqe *sqe;
    if ((sqe = io_uring_get_sqe(uring)) == nullptr) {
      return false;
    }
    io_uring_prep_recv(sqe, m_fd, m_buffer, m_length, m_flags);
    set_data(sqe);
    return true;
  }
};

class io_uring_op_accept_t : public io_uring_future {
  int m_fd;
  sockaddr *m_client_info;
  socklen_t *m_socklen;
  int m_flags;

public:
  io_uring_op_accept_t() = default;

  io_uring_op_accept_t(const int &fd, sockaddr *const &client_info,
                       socklen_t *const &socklen, const int &flags)
      : m_fd{fd}
      , m_client_info{client_info}
      , m_socklen{socklen}
      , m_flags{flags} {}

  bool run(io_uring *const uring) {
    io_uring_sqe *sqe;
    if ((sqe = io_uring_get_sqe(uring)) == nullptr) {
      return false;
    }
    io_uring_prep_accept(sqe, m_fd, m_client_info, m_socklen, 0);
    set_data(sqe);
    return true;
  }
};

class io_uring_op_send_t : public io_uring_future {
  int m_fd;
  void *m_buffer;
  size_t m_length;
  int m_flags;

public:
  io_uring_op_send_t() = default;

  io_uring_op_send_t(const int &fd, void *const &buffer, const size_t &length,
                     const int &flags)
      : m_fd{fd}, m_buffer{buffer}, m_length{length}, m_flags{flags} {}

  bool run(io_uring *const uring) {
    io_uring_sqe *sqe;
    if ((sqe = io_uring_get_sqe(uring)) == nullptr) {
      return false;
    }
    io_uring_prep_send(sqe, m_fd, m_buffer, m_length, m_flags);
    set_data(sqe);
    return true;
  }
};

class io_uring_op_close_t : public io_uring_future {
  int m_fd;
  uring_data *m_data;

public:
  io_uring_op_close_t() = default;

  io_uring_op_close_t(const int &fd) : m_fd{fd} {}

  bool run(io_uring *const uring) {
    io_uring_sqe *sqe;
    if ((sqe = io_uring_get_sqe(uring)) == nullptr) {
      return false;
    }
    io_uring_prep_close(sqe, m_fd);
    set_data(sqe);
    return true;
  }
};

using io_operation =
    std::variant<io_uring_op_sleep_t, io_uring_op_openat_t, io_uring_op_read_t,
                 io_uring_op_write_t, io_uring_op_recv_t, io_uring_op_accept_t,
                 io_uring_op_send_t, io_uring_op_close_t>;

#endif