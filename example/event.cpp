#include <iostream>

#include "coroutine/async.hpp"
#include "coroutine/event.hpp"
#include "coroutine/launch.hpp"
#include "coroutine/scheduler/scheduler.hpp"
#include "coroutine/task.hpp"

async<void> print_task(event &e) {
  for (int i = 0; i < 10; ++i) {
    sleep(1);
    e.set(i);
  }
  co_return;
}

launch<int> launch_coroutine() {
  event e;
  auto t = print_task(e).schedule_on(co_await get_scheduler());
  for (int i = 0; i < 10; ++i) {
    std::cout << co_await e << std::endl;
  }
  co_await t;
  co_return 0;
}

int main(int argc, char **argv) {
  scheduler schd;

  int val = launch_coroutine().schedule_on(&schd);

  return 0;
}