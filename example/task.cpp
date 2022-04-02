#include <iostream>

#include "coroutine/launch.hpp"
#include "coroutine/scheduler/scheduler.hpp"
#include "coroutine/task.hpp"

task<void> print_task() {
  std::cout << "print_task()\n";
  co_return;
}

launch<> launch_coroutine() {
  std::cout << "Launch Coroutine\n";
  co_await print_task();
}

task<int> add_number(int a, int b) {
  std::cout << "add a+b\n";
  co_return a + b;
}

launch<int> launch_coroutine_add() {
  std::cout << "Launch Coroutine Add\n";
  co_return co_await add_number(10, 20);
}

int main(int argc, char **argv) {
  scheduler schd;

  launch_coroutine().schedule_on(&schd);
  auto cr = launch_coroutine_add().schedule_on(&schd);
  sleep(1);
  std::cout << cr << std::endl;
  sleep(2);

  return 0;
}