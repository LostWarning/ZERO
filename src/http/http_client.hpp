#ifndef __HTTP_HTTP_CLIENT_HPP__
#define __HTTP_HTTP_CLIENT_HPP__

#include "coroutine/task.hpp"
#include "http_request.hpp"
#include "io/io_service.hpp"

#include <coroutine>

namespace http {

template <typename IO, size_t BUFFER_SIZE>
class http_client {
  using request = http_request<BUFFER_SIZE>;

public:
  struct accept_awaiter {
    accept_awaiter(http_client<IO, BUFFER_SIZE> *client) : m_client{client} {}

    http_client<IO, BUFFER_SIZE> *m_client;
    scheduler *m_schd;
    request *req;

    bool await_ready() noexcept {
      return m_client->has_pending_request() || !m_client->is_alive();
    }

    auto await_suspend(const std::coroutine_handle<> &handle) noexcept {
      m_client->m_scheduler       = m_schd;
      m_client->m_request_handler = handle;
      m_client->m_request_handler_ctl.store(true, std::memory_order_release);

      if (m_client->has_pending_request() &&
          m_client->m_request_handler_ctl.exchange(false,
                                                   std::memory_order_relaxed)) {
        return handle;
      }

      return m_schd->get_next_coroutine();
    }
    auto await_resume() noexcept { return m_client->get_next_request(); }

    void via(scheduler *s) { m_schd = s; }
  };

  IO *m_io        = nullptr;
  int m_client_fd = 0;

public:
  scheduler *m_scheduler;
  std::vector<request *> m_requests;
  std::coroutine_handle<> m_request_handler;
  std::atomic_bool m_request_handler_ctl = false;
  unsigned long m_processed_req          = 0;

  std::atomic_bool m_alive = true;

  http_client(IO *io, int client_fd) : m_io{io}, m_client_fd{client_fd} {}

  task<> start() {
    size_t len   = 0;
    request *req = new request;
    while (m_alive.load(std::memory_order_relaxed)) {
      len += co_await m_io->recv(m_client_fd, req->m_header.m_data + len,
                                 req->m_header.get_buffer_size(), 0);
      if (len <= 0) {
        m_alive.store(false, std::memory_order_relaxed);
      } else if (!req->m_header.parse()) {
        if (len >= BUFFER_SIZE) {
          m_alive.store(false, std::memory_order_relaxed);
        } else {
          continue;
        }
      } else {
        len = 0;
        m_requests.push_back(req);
        req = new request;
      }

      if (m_request_handler_ctl.exchange(false, std::memory_order_acquire)) {
        m_scheduler->schedule(m_request_handler);
      }
    }
    delete req;
    if (m_request_handler_ctl.exchange(false, std::memory_order_acquire)) {
      m_scheduler->schedule(m_request_handler);
    }
    co_return;
  }

  auto is_alive() const noexcept {
    return this->m_alive.load(std::memory_order_relaxed);
  }

  auto get_request() { return accept_awaiter(this); }

  auto has_pending_request() { return m_requests.size() != m_processed_req; }

  auto get_next_request() {
    return m_alive ? m_requests[m_processed_req++] : nullptr;
  }

  auto send(void *const &buffer, const size_t &length, const int &flags) {
    return m_io->send(m_client_fd, buffer, length, flags);
  }

  auto close() { return m_io->close(m_client_fd); }
};

} // namespace http

#endif