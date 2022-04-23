#ifndef __COROUTINE_TIMER_HPP__
#define __COROUTINE_TIMER_HPP__

#include "async.hpp"
#include "io/io_service.hpp"

#include <functional>
#include <sys/timerfd.h>

// async<void> timer(io_service *io, itimerspec spec, std::function<void()>
// func);

template <typename Awaitable>
async<void> timer(io_service *io, itimerspec spec, Awaitable awaitable) {
  int tfd = timerfd_create(CLOCK_REALTIME, 0);
  timerfd_settime(tfd, 0, &spec, NULL);
  std::stop_token st = co_await get_stop_token();
  std::stop_callback scb(st, [&] { io->close(tfd); });

  while (!st.stop_requested()) {
    unsigned long u;
    co_await io->read(tfd, &u, sizeof(u), 0);
    co_await awaitable();
  }
  co_await io->close(tfd);
}

template <typename Awaitable>
async<typename Awaitable::Return> delayed(io_service *io, unsigned long sec,
                                          unsigned long nsec,
                                          Awaitable awaitable) {
  int tfd = timerfd_create(CLOCK_REALTIME, 0);
  itimerspec spec;
  spec.it_value.tv_sec     = sec;
  spec.it_value.tv_nsec    = nsec;
  spec.it_interval.tv_sec  = 0;
  spec.it_interval.tv_nsec = 0;
  timerfd_settime(tfd, 0, &spec, NULL);

  std::stop_token st = co_await get_stop_token();
  std::stop_callback scb(st, [&] { io->close(tfd); });

  unsigned long u;
  co_await io->read(tfd, &u, sizeof(u), 0);
  co_await io->close(tfd);
  co_return co_await awaitable;
}

template <typename Return>
async<Return> delayed(io_service *io, unsigned long sec, unsigned long nsec,
                      std::function<Return()> func) {
  int tfd = timerfd_create(CLOCK_REALTIME, 0);
  itimerspec spec;
  spec.it_value.tv_sec     = sec;
  spec.it_value.tv_nsec    = nsec;
  spec.it_interval.tv_sec  = 0;
  spec.it_interval.tv_nsec = 0;
  timerfd_settime(tfd, 0, &spec, NULL);

  std::stop_token st = co_await get_stop_token();
  std::stop_callback scb(st, [&] { io->close(tfd); });

  unsigned long u;
  co_await io->read(tfd, &u, sizeof(u), 0);
  co_await io->close(tfd);
  co_return func();
}
#endif