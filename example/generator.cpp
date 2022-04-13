#include <iostream>

#include "coroutine/generator.hpp"
#include "coroutine/launch.hpp"
#include "coroutine/scheduler/scheduler.hpp"

generator<int> integers() {
  auto st = co_await get_stop_token();
  int i   = 0;
  while (!st.stop_requested()) {
    sleep(1);
    co_yield i++;
  }
}

launch<int> launch_coroutine() {
  auto ints = integers();
  while (ints) {
    int val = co_await ints;
    std::cout << val << std::endl;
    if (val == 10) {
      co_await ints.cancel();
    }
  }
  co_return 1;
}

int main(int argc, char **argv) {
  scheduler schd;
  int a = launch_coroutine().schedule_on(&schd);

  return 0;
}