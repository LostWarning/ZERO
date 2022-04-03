#ifndef __UTILS_FIFO_ALLOCATOR_HPP__
#define __UTILS_FIFO_ALLOCATOR_HPP__

#include <atomic>
#include <iostream>
#include <vector>

template <unsigned int size>
union chunk_item {
  char m_data[size];
  chunk_item *m_next = nullptr;
};

template <typename T, unsigned int block_size>
class pool_allocator {
  using chunk = chunk_item<sizeof(T)>;
  struct block_ptr {
    chunk *m_ptr       = nullptr;
    unsigned int m_tag = 0;
  };
  std::atomic<block_ptr> m_allocator_head;
  std::vector<chunk *> m_allocated_blocks{nullptr};

  std::atomic_bool m_block_allocating{false};

public:
  pool_allocator() {
    block_ptr ptr{nullptr, 0};
    m_allocator_head.store(ptr);
    allocate_block();
  }
  ~pool_allocator() {
    for (auto &block : m_allocated_blocks) {
      delete[] block;
    }
  }

  void allocate_block() {
    if (m_block_allocating.exchange(true) == false) {
      chunk *new_block = new chunk[block_size];
      m_allocated_blocks.push_back(new_block);
      for (unsigned int i = 0; i < block_size - 1; ++i) {
        new_block[i].m_next = &new_block[i + 1];
      }
      block_ptr new_head{new_block, 0};
      block_ptr curr_head = m_allocator_head.load();
      do {
        new_block[block_size - 1].m_next = curr_head.m_ptr;
        new_head.m_tag                   = curr_head.m_tag + 1;
      } while (!m_allocator_head.compare_exchange_strong(curr_head, new_head));

      m_block_allocating.store(false);
    }
  }

  template <typename O = T>
  O *allocate() {
    block_ptr head = m_allocator_head.load();
    block_ptr new_head{nullptr, 0};
    do {
      while (head.m_ptr == nullptr) {
        allocate_block();
        head = m_allocator_head.load();
      }
      new_head.m_ptr = head.m_ptr->m_next;
      new_head.m_tag = head.m_tag + 1;
    } while (!m_allocator_head.compare_exchange_strong(head, new_head));

    return new (head.m_ptr) O;
  }

  template <typename O = T, typename... Args>
  O *allocate(Args &&...args) {
    block_ptr head = m_allocator_head.load();
    block_ptr new_head{nullptr, 0};
    do {
      while (head.m_ptr == nullptr) {
        allocate_block();
        head = m_allocator_head.load();
      }
      new_head.m_ptr = head.m_ptr->m_next;
      new_head.m_tag = head.m_tag + 1;
    } while (!m_allocator_head.compare_exchange_strong(head, new_head));

    return new (head.m_ptr) O(args...);
  }

  template <typename O = T>
  void deallocate(O *ptr) {
    ptr->~O();
    block_ptr new_head{reinterpret_cast<chunk *>(ptr), 0};
    block_ptr head = m_allocator_head.load();
    do {
      new_head.m_ptr->m_next = head.m_ptr;
      new_head.m_tag         = head.m_tag + 1;
    } while (!m_allocator_head.compare_exchange_strong(head, new_head));
  }
};

#endif