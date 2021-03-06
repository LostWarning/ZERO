#include "io_service.hpp"

thread_local unsigned int io_service::m_thread_id                    = 0;
thread_local uring_data::allocator *io_service::m_uio_data_allocator = nullptr;
thread_local io_op_pipeline *io_service::m_io_queue                  = nullptr;

io_service::io_service(const u_int &entries, const u_int &flags)
    : io_operation(this), m_entries(entries), m_flags(flags) {
  io_uring_queue_init(entries, &m_uring, flags);
  m_io_cq_thread = std::move(std::thread([&] { this->io_loop(); }));
}

io_service::io_service(const u_int &entries, io_uring_params &params)
    : io_operation(this), m_entries(entries) {
  io_uring_queue_init_params(entries, &m_uring, &params);
  m_io_cq_thread = std::move(std::thread([&] { this->io_loop(); }));
}

io_service::~io_service() {
  m_stop_requested.store(true, std::memory_order_relaxed);
  nop(IOSQE_IO_DRAIN);
  m_io_cq_thread.join();
}

void io_service::io_loop() noexcept {
  while (!m_stop_requested.load(std::memory_order_relaxed)) {
    io_uring_cqe *cqe = nullptr;
    if (io_uring_wait_cqe(&m_uring, &cqe) == 0) {
      handle_completion(cqe);
      io_uring_cqe_seen(&m_uring, cqe);
    } else {
      std::cerr << "Wait CQE Failed\n";
    }
    unsigned completed = 0;
    unsigned head;

    io_uring_for_each_cqe(&m_uring, head, cqe) {
      ++completed;
      handle_completion(cqe);
    }
    if (completed) {
      io_uring_cq_advance(&m_uring, completed);
    }
    if (m_io_queue_overflow.load(std::memory_order_relaxed) != nullptr)
        [[unlikely]] {
      submit();
    }
  }
  io_uring_queue_exit(&m_uring);
  return;
}

bool io_service::io_queue_empty() const noexcept {
  bool is_empty = true;
  for (auto &q : this->m_io_queues) {
    is_empty = is_empty & q->empty();
  }
  return is_empty;
}

void io_service::setup_thread_context() {
  if (m_thread_id == 0) {
    m_thread_id = m_threads.fetch_add(1, std::memory_order_relaxed) + 1;
    m_uio_data_allocator = new uring_data::allocator;
    m_uio_data_allocators.push_back(m_uio_data_allocator);
    m_io_queue = new io_op_pipeline(128);
    m_io_queues.push_back(m_io_queue);
  }
}

void io_service::submit() {
  while (!io_queue_empty() &&
         !m_io_sq_running.exchange(true, std::memory_order_seq_cst)) {
    auto overflow_queue = m_io_queue_overflow.load(std::memory_order_relaxed);
    if (overflow_queue != nullptr) [[unlikely]] {
      int res = overflow_queue->init_io_uring_ops(&m_uring);
      if (res >= 0) {
        m_io_queue_overflow.store(nullptr, std::memory_order_relaxed);
      } else {
        return;
      }
    }

    unsigned int completed = 0;
    while (!io_queue_empty()) {
      for (auto &q : m_io_queues) {
        int res = q->init_io_uring_ops(&m_uring);
        if (res < 0) [[unlikely]] {
          m_io_queue_overflow.store(q, std::memory_order_relaxed);
          return;
        } else {
          completed += res;
        }
      }
    }
    if (completed) {
      io_uring_submit(&m_uring);
    }
    m_io_sq_running.store(false, std::memory_order_relaxed);
  }
}

void io_service::handle_completion(io_uring_cqe *cqe) {
  auto data = static_cast<uring_data *>(io_uring_cqe_get_data(cqe));
  if (data != nullptr) {
    data->m_result = cqe->res;
    data->m_flags  = cqe->flags;
    if (data->m_handle_ctl.exchange(true, std::memory_order_acq_rel)) {
      data->m_scheduler->schedule(data->m_handle);
    }
    if (data->m_destroy_ctl.exchange(true, std::memory_order_relaxed)) {
      data->destroy();
    }
  }
}

void io_service::submit(io_batch<io_service> &batch) {
  m_io_queue->enqueue(batch.operations());
  submit();
}

void io_service::submit(io_link<io_service> &io_link) {
  auto &operations = io_link.operations();
  size_t op_count  = operations.size() - 1;

  for (size_t i = 0; i < op_count; ++i) {
    std::visit([](auto &&op) { op.m_sqe_flags |= IOSQE_IO_HARDLINK; },
               operations[i]);
  }

  m_io_queue->enqueue(operations);

  submit();
}
