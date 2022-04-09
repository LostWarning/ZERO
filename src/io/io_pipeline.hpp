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

    auto submit_operation = [&uring](IO_URING_OP auto &&item) {
      return item.run(uring);
    };

    unsigned int completed = 0;
    if (m_has_overflow_work) {
      if (!std::visit(submit_operation, m_overflow_work)) {
        std::cerr << "Queue Full\n";
      } else {
        m_has_overflow_work = false;
        ++completed;
      }
    }

    io_operation op;
    while (m_io_work_queue.dequeue(op)) {
      if (!std::visit(submit_operation, op)) {
        std::cerr << "Queue Full\n";
        m_overflow_work     = op;
        m_has_overflow_work = true;
      } else {
        ++completed;
      }
    }
    return completed;
  }

  bool empty() const noexcept { return m_io_work_queue.empty(); }
};

#endif