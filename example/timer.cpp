#include "coroutine/timer.hpp"
#include "coroutine/async.hpp"
#include "coroutine/launch.hpp"
#include "coroutine/scheduler/scheduler.hpp"
#include "io/io_service.hpp"
#include <string.h>

async<int> print() {
  std::cout << "Print\n";
  co_return 2;
}

launch<> coroutine_2(io_service *io) {
  std::cerr << "Calling delayed\n";
  int a = co_await delayed<int>(io, 5, 0, [] {
    std::cerr << "Run delayed task\n";
    return 101;
  });
  std::cout << a << std::endl;

  co_await delayed(io, 5, 0, print());

  auto t = timer(io, {{1, 0}, {5, 0}}, []() -> async<> {
    std::cerr << "Hello World\n";
    co_return;
  });

  co_await t;
}

int main(int, char **) {
  scheduler scheduler;
  io_service io(100, 0);

  coroutine_2(&io).schedule_on(&scheduler).join();

  return 0;
}