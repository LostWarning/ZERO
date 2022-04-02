#ifndef __IO_IO_PIPELINE_HPP__
#define __IO_IO_PIPELINE_HPP__

#include "io_operations.hpp"
#include "queue/pool_allocator.hpp"
#include "uring_data.hpp"

#include <atomic>
#include <iostream>
#include <liburing.h>
#include <linux/time_types.h>
#include <sys/socket.h>
#include <unistd.h>

class io_op_pipeline {
  struct block_ptr {
    io_operation *m_ptr = nullptr;
    unsigned int m_tag  = 0;
  };
  std::atomic<block_ptr> m_head;
  pool_allocator<io_operation, 248> m_allocator;

protected:
  bool run_and_release(io_operation *op, io_uring *const uring) {
    switch (op->m_type) {
    case IO_OP_TYPE::SLEEP:
      if (!op->m_sleep.m_item.run(uring)) {
        return false;
      }
      m_allocator.deallocate(&op->m_sleep);
      break;
    case IO_OP_TYPE::OPEN_AT:
      if (!op->m_openat.m_item.run(uring)) {
        return false;
      }
      m_allocator.deallocate(&op->m_openat);
      break;
    case IO_OP_TYPE::READ:
      if (!op->m_read.m_item.run(uring)) {
        return false;
      }
      m_allocator.deallocate(&op->m_read);
      break;
    case IO_OP_TYPE::WRITE:
      if (!op->m_write.m_item.run(uring)) {
        return false;
      }
      m_allocator.deallocate(&op->m_write);
      break;
    case IO_OP_TYPE::RECV:
      if (!op->m_recv.m_item.run(uring)) {
        return false;
      }
      m_allocator.deallocate(&op->m_recv);
      break;
    case IO_OP_TYPE::ACCEPT:
      if (!op->m_accept.m_item.run(uring)) {
        return false;
      }
      m_allocator.deallocate(&op->m_accept);
      break;
    case IO_OP_TYPE::SEND:
      if (!op->m_send.m_item.run(uring)) {
        return false;
      }
      m_allocator.deallocate(&op->m_send);
      break;
    case IO_OP_TYPE::CLOSE:
      if (!op->m_close.m_item.run(uring)) {
        return false;
      }
      m_allocator.deallocate(&op->m_close);
      break;
    }
    return true;
  }

  void enqueue_(io_operation *i) {
    block_ptr head = m_head.load();
    block_ptr new_head{i, 0};
    do {
      reinterpret_cast<io_operation_next *>(new_head.m_ptr)->m_next =
          head.m_ptr;
      new_head.m_tag = head.m_tag + 1;
    } while (!m_head.compare_exchange_strong(head, new_head));
  }

public:
  io_op_pipeline() {}

  template <typename T>
  void enqueue(const T &val) {
    T *i = m_allocator.allocate<T>();
    *i   = std::move(val);

    enqueue_(reinterpret_cast<io_operation *>(i));
  }

  unsigned int init_io_uring_ops(io_uring *const uring) {
    unsigned int count = 0;
    do {
      block_ptr head = m_head.load();
      block_ptr new_head;
      do {
        if (head.m_ptr == nullptr) {
          return count;
        }
        new_head.m_ptr =
            reinterpret_cast<io_operation_next *>(head.m_ptr)->m_next;
        new_head.m_tag = head.m_tag + 1;
      } while (!m_head.compare_exchange_strong(head, new_head));

      if (!run_and_release(head.m_ptr, uring)) {
        std::cerr << "Queue Full\n";
        enqueue_(head.m_ptr);
        return count;
      }
      ++count;
    } while (true);
    return count;
  }

  bool empty() {
    return m_head.load(std::memory_order_relaxed).m_ptr == nullptr;
  }
};

#endif