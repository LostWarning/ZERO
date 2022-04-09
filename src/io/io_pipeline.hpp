#ifndef __IO_IO_PIPELINE_HPP__
#define __IO_IO_PIPELINE_HPP__

#include "io_operations.hpp"
#include "queue/io_work_queue.hpp"
#include "uring_data.hpp"

#include <atomic>
#include <iostream>
#include <liburing.h>
#include <linux/time_types.h>
#include <sys/socket.h>
#include <type_traits>
#include <unistd.h>

class io_op_pipeline {
  io_work_queue<io_operation> m_io_work_queue;
  io_operation m_overflow_work;
  bool m_has_overflow_work = false;

public:
  io_op_pipeline(size_t capacity) : m_io_work_queue(capacity) {}

  template <typename T>
  void enqueue(T &&val) {
    m_io_work_queue.enqueue(io_operation(std::forward<T>(val)));
  }

  unsigned int init_io_uring_ops(io_uring *const uring) {

    bool status;
    if (m_has_overflow_work) {
      std::visit([&](IO_URING_OP auto &&item) { status = item.run(uring); },
                 m_overflow_work);
      if (!status) {
        std::cerr << "Queue Full\n";
        return 0;
      } else {
        m_has_overflow_work = false;
      }
    }

    unsigned int count = 0;
    io_operation op;
    while (m_io_work_queue.dequeue(op)) {
      std::visit([&](IO_URING_OP auto &&item) { status = item.run(uring); },
                 op);
      if (!status) {
        std::cerr << "Queue Full\n";
        m_overflow_work     = op;
        m_has_overflow_work = true;
        return count;
      }
      ++count;
    }
    return count;
  }

  bool empty() const noexcept { return m_io_work_queue.empty(); }
};

#endif