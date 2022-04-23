#include <iostream>

#include "coroutine/launch.hpp"
#include "coroutine/scheduler/scheduler.hpp"

launch<> hello() {
  std::cout << "Hello Coroutine\n";
  co_return;
}

launch<int> add(int a, int b) { co_return a + b; }

int main(int argc, char **argv) {
  scheduler schd;

  auto cr1 = hello().schedule_on(&schd); // Run coroutine on the scheduler
  cr1.join();                            // Wait for the coroutine finish

  // When returning a value we can use convertion operator to wait and get the
  // value
  int cr2 = add(10, 20).schedule_on(&schd);
  std::cout << cr2 << std::endl;

  return 0;
}