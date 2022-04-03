#ifndef __UTILS_QUEUE_HPP__
#define __UTILS_QUEUE_HPP__

#include "pool_allocator.hpp"

#include <atomic>
#include <iostream>

#include <mutex>
#include <vector>

template <typename T>
struct item_container {
  T m_item;
  item_container *m_next = nullptr;
};

template <typename T>
class lock_free_stack {
  using item = item_container<T>;
  struct block_ptr {
    item *m_ptr        = nullptr;
    unsigned int m_tag = 0;
  };
  std::atomic<block_ptr> m_head;

  pool_allocator<item, 1024> m_allocator;

public:
  lock_free_stack() {
    block_ptr ptr{nullptr, 0};
    m_head.store(ptr);
  }
  void enqueue(const T &val) {
    item *i   = m_allocator.allocate();
    i->m_item = std::move(val);

    block_ptr head = m_head.load();
    block_ptr new_head{i, 0};
    do {
      new_head.m_ptr->m_next = head.m_ptr;
      new_head.m_tag         = head.m_tag + 1;
    } while (!m_head.compare_exchange_strong(head, new_head));
  }

  bool dequeue(T &val) {
    block_ptr head = m_head.load();
    block_ptr new_head;
    do {
      if (head.m_ptr == nullptr) {
        return false;
      }
      new_head.m_ptr = head.m_ptr->m_next;
      new_head.m_tag = head.m_tag + 1;
    } while (!m_head.compare_exchange_strong(head, new_head));
    val = std::move(head.m_ptr->m_item);
    m_allocator.deallocate(head.m_ptr);
    return true;
  }

  bool empty() {
    return m_head.load(std::memory_order_relaxed).m_ptr == nullptr;
  }
};

#endif