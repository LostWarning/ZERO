#include "coroutine/async.hpp"
#include "coroutine/launch.hpp"
#include "coroutine/scheduler/scheduler.hpp"
#include "io/io_service.hpp"
#include <fcntl.h>
#include <string.h>

async<int> print_file(io_service &io, int dir, const std::string filename) {
  char buffer[256]{0};
  int fd = co_await io.openat(dir, filename.c_str(), 0, 0);
  co_return co_await io.read(fd, buffer, 256, 0);
}

launch<int> coroutine_1(io_service &io, int dir) {
  auto scheduler       = co_await get_scheduler();
  auto print_file_task = print_file(io, dir, "log");
  co_return co_await print_file_task.schedule_on(scheduler);
}

launch<int> coroutine_2(io_service &io, int dir) {
  co_return co_await print_file(io, dir, "response");
}

int main(int, char **) {
  scheduler scheduler;
  io_service io(100, 0);

  int dir = open(".", 0);

  auto cr1 = coroutine_1(io, dir).schedule_on(&scheduler);
  auto cr2 = coroutine_2(io, dir).schedule_on(&scheduler);

  std::cout << "File Length 1 : " << cr1 << std::endl;
  std::cout << "File Length 2 : " << cr2 << std::endl;

  return 0;
}