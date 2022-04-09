#include "io_service.hpp"

thread_local unsigned int io_service::m_thread_id                    = 0;
thread_local uring_data::allocator *io_service::m_uio_data_allocator = nullptr;

io_service::io_service(const u_int &entries, const u_int &flags)
    : m_entries(entries), m_flags(flags) {
  io_uring_queue_init(entries, &m_uring, flags);
  m_io_cq_thread = std::move(std::thread([&] { this->io_loop(); }));

  m_io_queues.resize(32);
  for (size_t i = 0; i < 32; ++i) {
    m_io_queues[i] = new io_op_pipeline(128);
  }
}

io_service::~io_service() { io_uring_queue_exit(&m_uring); }

unsigned int io_service::drain_io_results() {
  unsigned completed = 0;
  unsigned head;
  struct io_uring_cqe *cqe;

  io_uring_for_each_cqe(&m_uring, head, cqe) {
    ++completed;
    handle_completion(cqe);
  }
  if (completed) {
    io_uring_cq_advance(&m_uring, completed);
  }
  return completed;
}

void io_service::io_loop() noexcept {
  while (true) {
    io_uring_cqe *cqe = nullptr;
    if (io_uring_wait_cqe(&m_uring, &cqe) == 0) {
      handle_completion(cqe);
      io_uring_cqe_seen(&m_uring, cqe);
    } else {
      std::cerr << "Wait CQE Failed\n";
    }

    drain_io_results();
  }
  return;
}

bool io_service::io_queue_empty() const noexcept {
  bool is_empty = true;
  for (auto &q : this->m_io_queues) {
    is_empty = is_empty && q->empty();
  }
  return is_empty;
}

void io_service::setup_uring_allocator() {
  if (m_thread_id == 0) {
    m_thread_id = m_io_queue_count.fetch_add(1, std::memory_order_relaxed) + 1;
    m_uio_data_allocator = new uring_data::allocator;
    m_uio_data_allocators.push_back(m_uio_data_allocator);
  }
}

void io_service::submit() {
  while (!io_queue_empty() &&
         !m_io_sq_running.exchange(true, std::memory_order_seq_cst)) {

    unsigned int completed = 0;
    while (!io_queue_empty()) {
      for (auto &q : m_io_queues) {
        completed += q->init_io_uring_ops(&m_uring);
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
  unsigned int completed = 0;
  auto &operations       = batch.operations();
  size_t op_count        = operations.size();
  if (!m_io_sq_running.exchange(true, std::memory_order_relaxed)) {
    for (auto &op : operations) {
      if (std::visit(
              [&](IO_URING_OP auto &&item) { return item.run(&m_uring); },
              op)) {
        ++completed;
      }
    }
    m_io_sq_running.store(false, std::memory_order_relaxed);
  }
  if (completed != operations.size()) {
    for (size_t i = completed; i < op_count; ++i) {
      m_io_queues[m_thread_id - 1]->enqueue(operations[i]);
    }
  }

  submit();
}
