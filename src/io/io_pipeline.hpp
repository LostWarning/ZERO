#ifndef __IO_IO_PIPELINE_HPP__
#define __IO_IO_PIPELINE_HPP__

#include "io_uring_op.hpp"
#include "queue/io_work_queue.hpp"
#include "uring_data.hpp"

class io_op_pipeline {
  io_work_queue<io_uring_op> m_io_work_queue;
  io_uring_op m_overflow_work;
  bool m_has_overflow_work = false;

public:
  io_op_pipeline(size_t capacity) : m_io_work_queue(capacity) {}

  template <typename T>
  void enqueue(T &&val) {
    m_io_work_queue.enqueue(io_uring_op(std::forward<T>(val)));
  }

  void enqueue(std::vector<io_uring_op> &items) {
    m_io_work_queue.bulk_enqueue(items);
  }

  int init_io_uring_ops(io_uring *const uring) {

    auto submit_operation = [&uring](IO_URING_OP auto &&item) {
      return item.run(uring);
    };

    unsigned int completed = 0;
    if (m_has_overflow_work) {
      if (!std::visit(submit_operation, m_overflow_work)) {
        return -1;
      } else {
        m_has_overflow_work = false;
        ++completed;
      }
    }

    io_uring_op op;
    while (m_io_work_queue.dequeue(op)) {
      if (!std::visit(submit_operation, op)) {
        return -1;
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