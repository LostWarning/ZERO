#include "timer.hpp"

async<void> timer(io_service *io, itimerspec spec, std::function<void()> func) {
  int tfd = timerfd_create(CLOCK_REALTIME, 0);
  timerfd_settime(tfd, 0, &spec, NULL);
  std::stop_token st = co_await get_stop_token();
  std::stop_callback scb(st, [&] { io->close(tfd); });

  while (!st.stop_requested()) {
    std::cerr << "Waiting for timer to trigger\n";
    unsigned long u;
    co_await io->read(tfd, &u, sizeof(u), 0);
    func();
  }
  co_await io->close(tfd);
}
