#ifndef __IO_IO_OPERATIONS_HPP__
#define __IO_IO_OPERATIONS_HPP__

#include "uring_data.hpp"

enum class IO_OP_TYPE {
  OPEN_AT,
  READ,
  WRITE,
  RECV,
  SEND,
  ACCEPT,
  CLOSE,
  SLEEP
};

struct io_uring_op_openat_t {
  int m_dir;
  const char *m_filename;
  int m_flags;
  mode_t m_mode;
  uring_data *m_data;

  bool run(io_uring *const uring) {
    io_uring_sqe *sqe;
    if ((sqe = io_uring_get_sqe(uring)) == nullptr) {
      return false;
    }
    io_uring_prep_openat(sqe, m_dir, m_filename, m_flags, m_mode);
    io_uring_sqe_set_data(sqe, m_data);
    return true;
  }
};

struct io_uring_op_read_t {
  int m_fd;
  void *m_buffer;
  unsigned m_bytes;
  off_t m_offset;
  uring_data *m_data;

  bool run(io_uring *const uring) {
    io_uring_sqe *sqe;
    if ((sqe = io_uring_get_sqe(uring)) == nullptr) {
      return false;
    }
    io_uring_prep_read(sqe, m_fd, m_buffer, m_bytes, m_offset);
    io_uring_sqe_set_data(sqe, m_data);
    return true;
  }
};

struct io_uring_op_write_t {
  int m_fd;
  void *m_buffer;
  unsigned m_bytes;
  off_t m_offset;
  uring_data *m_data;

  bool run(io_uring *const uring) {
    io_uring_sqe *sqe;
    if ((sqe = io_uring_get_sqe(uring)) == nullptr) {
      return false;
    }
    io_uring_prep_write(sqe, m_fd, m_buffer, m_bytes, m_offset);
    io_uring_sqe_set_data(sqe, m_data);
    return true;
  }
};

struct io_uring_op_sleep_t {
  __kernel_timespec *m_time;
  uring_data *m_data;

  bool run(io_uring *const uring) {
    io_uring_sqe *sqe;
    if ((sqe = io_uring_get_sqe(uring)) == nullptr) {
      return false;
    }
    io_uring_prep_timeout(sqe, m_time, 0, 0);
    io_uring_sqe_set_data(sqe, m_data);
    return true;
  }
};

struct io_uring_op_recv_t {
  int m_fd;
  void *m_buffer;
  size_t m_length;
  int m_flags;
  uring_data *m_data;

  bool run(io_uring *const uring) {
    io_uring_sqe *sqe;
    if ((sqe = io_uring_get_sqe(uring)) == nullptr) {
      return false;
    }
    io_uring_prep_recv(sqe, m_fd, m_buffer, m_length, m_flags);
    io_uring_sqe_set_data(sqe, m_data);
    return true;
  }
};

struct io_uring_op_accept_t {
  int m_fd;
  sockaddr *m_client_info;
  socklen_t *m_socklen;
  int m_flags;
  uring_data *m_data;

  bool run(io_uring *const uring) {
    io_uring_sqe *sqe;
    if ((sqe = io_uring_get_sqe(uring)) == nullptr) {
      return false;
    }
    io_uring_prep_accept(sqe, m_fd, m_client_info, m_socklen, 0);
    io_uring_sqe_set_data(sqe, m_data);
    return true;
  }
};

struct io_uring_op_send_t {
  int m_fd;
  void *m_buffer;
  size_t m_length;
  int m_flags;
  uring_data *m_data;

  bool run(io_uring *const uring) {
    io_uring_sqe *sqe;
    if ((sqe = io_uring_get_sqe(uring)) == nullptr) {
      return false;
    }
    io_uring_prep_send(sqe, m_fd, m_buffer, m_length, m_flags);
    io_uring_sqe_set_data(sqe, m_data);
    return true;
  }
};

struct io_uring_op_close_t {
  int m_fd;
  uring_data *m_data;

  bool run(io_uring *const uring) {
    io_uring_sqe *sqe;
    if ((sqe = io_uring_get_sqe(uring)) == nullptr) {
      return false;
    }
    io_uring_prep_close(sqe, m_fd);
    io_uring_sqe_set_data(sqe, m_data);
    return true;
  }
};

union io_operation;
struct io_operation_next {
  IO_OP_TYPE m_type;
  io_operation *m_next;
};

template <typename ITEM>
struct io_operation_item {
  io_operation_next m_next_op;
  ITEM m_item;
};
union io_operation {
  IO_OP_TYPE m_type;
  io_operation_next m_next_op;
  io_operation_item<io_uring_op_sleep_t> m_sleep;
  io_operation_item<io_uring_op_openat_t> m_openat;
  io_operation_item<io_uring_op_read_t> m_read;
  io_operation_item<io_uring_op_write_t> m_write;
  io_operation_item<io_uring_op_recv_t> m_recv;
  io_operation_item<io_uring_op_accept_t> m_accept;
  io_operation_item<io_uring_op_send_t> m_send;
  io_operation_item<io_uring_op_close_t> m_close;

  io_operation *m_next_chunk = nullptr;
};

#endif