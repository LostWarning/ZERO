#include <iostream>

#include "coroutine/launch.hpp"
#include "coroutine/scheduler/scheduler.hpp"
#include "coroutine/task.hpp"
#include "io/io_service.hpp"

#include <sys/timerfd.h>

task<void> print_task() {
  std::cout << "print_task()\n";
  co_return;
}

launch<> launch_coroutine(io_service *io) {
  std::cout << "Launch Coroutine\n";

  int tfd = timerfd_create(CLOCK_REALTIME, 0);

  itimerspec spec;
  spec.it_value.tv_sec     = 5;
  spec.it_value.tv_nsec    = 0;
  spec.it_interval.tv_sec  = 0;
  spec.it_interval.tv_nsec = 0;
  timerfd_settime(tfd, 0, &spec, NULL);

  std::cerr << "Going to wait on timer_fd\n";
  co_await io->poll_add(tfd, POLL_IN);
  std::cerr << "Poll Event received\n";

  co_await io->delay(5, 0);
}

int main(int argc, char **argv) {
  scheduler schd;
  io_service io(100, 0);

  auto cr = launch_coroutine(&io).schedule_on(&schd);
  cr.join();

  return 0;
}