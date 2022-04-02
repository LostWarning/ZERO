#ifndef __IO_URING_DATA_HPP__
#define __IO_URING_DATA_HPP__

#include "coroutine/scheduler/scheduler.hpp"

#include <atomic>
#include <coroutine>
#include <liburing.h>

struct uring_data {
  scheduler *m_scheduler = nullptr;
  std::coroutine_handle<> m_handle;
  int m_result;
  unsigned int m_flags;
  std::atomic_bool m_handle_ctl{false};
};

#endif