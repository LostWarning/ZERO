#ifndef __IO_IO_SERVICE_HPP__
#define __IO_IO_SERVICE_HPP__

#include "io_operations.hpp"
#include "io_pipeline.hpp"
#include "uring_data.hpp"

#include <atomic>
#include <coroutine>
#include <iostream>
#include <liburing.h>
#include <liburing/io_uring.h>
#include <memory>

class io_service : public io_operation<io_service> {
  static thread_local unsigned int m_thread_id;
  static thread_local uring_data::allocator *m_uio_data_allocator;
  static thread_local io_op_pipeline *m_io_queue;

  std::vector<io_op_pipeline *> m_io_queues;
  std::vector<uring_data::allocator *> m_uio_data_allocators;

  std::atomic<io_op_pipeline *> m_io_queue_overflow{nullptr};

  io_uring m_uring;

  unsigned int m_entries;
  unsigned int m_flags;

  std::thread m_io_cq_thread;
  std::atomic_bool m_io_sq_running{false};
  std::atomic_int m_threads{0};

  std::atomic_bool m_stop_requested{false};

public:
  io_service(const u_int &entries, const u_int &flags);
  io_service(const u_int &entries, io_uring_params &params);

  ~io_service();

  template <size_t n>
  bool register_buffer(iovec (&io_vec)[n]) {
    return io_uring_register_buffers(&m_uring, io_vec, n) == 0 ? true : false;
  }

  auto batch() { return io_batch<io_service>(this); }

  auto link() { return io_link<io_service>(this); }

  uring_data::allocator *get_awaiter_allocator() {
    setup_thread_context();
    return m_uio_data_allocator;
  }

  void submit(io_batch<io_service> &batch);

  void submit(io_link<io_service> &link);

  template <IO_URING_OP OP>
  auto submit_io(OP &&operation) -> uring_awaiter {

    setup_thread_context();

    auto future = operation.get_future(m_uio_data_allocator);

    m_io_queue->enqueue(std::forward<OP>(operation));

    submit();

    return future;
  }

  unsigned int get_buffer_index(unsigned int &flag) { return flag >> 16; }

protected:
  void submit();

  void io_loop() noexcept;

  bool io_queue_empty() const noexcept;

  void setup_thread_context();

  void handle_completion(io_uring_cqe *cqe);
};

#endif