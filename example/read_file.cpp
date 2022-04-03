#include "coroutine/launch.hpp"
#include "coroutine/scheduler/scheduler.hpp"
#include "coroutine/task.hpp"
#include "io/io_service.hpp"
#include <fcntl.h>

task<int> print_file(io_service &io, const std::string &filename) {
  char buffer[128]{0};
  int dir = open(".", 0);
  int fd  = co_await io.openat(dir, filename.c_str(), 0, 0);
  int len = co_await io.read(fd, buffer, 128, 0);
  std::cout << buffer << std::endl;
  co_return len;
}

launch<int> coroutine_1(io_service &io) {
  auto print_file_task = print_file(io, "log");
  co_return co_await print_file_task;
}

int main(int, char **) {
  scheduler scheduler;
  io_service io(100, 0);

  int len = coroutine_1(io).schedule_on(&scheduler);
  std::cout << "File Length : " << len << std::endl;

  return 0;
}