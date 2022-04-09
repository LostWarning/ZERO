#include "coroutine/launch.hpp"
#include "coroutine/scheduler/scheduler.hpp"
#include "coroutine/task.hpp"
#include "io/io_service.hpp"
#include <fcntl.h>

task<int> print_file(io_service &io) {
  char buffer1[128]{0};
  char buffer2[256]{0};
  int dir          = open(".", 0);
  auto batch_1     = io.batch();
  auto fd1_awaiter = batch_1.openat(dir, "log", 0, 0);
  auto fd2_awaiter = batch_1.openat(dir, "response", 0, 0);
  io.submit(batch_1);

  int fd1 = co_await fd1_awaiter;
  int fd2 = co_await fd2_awaiter;

  auto link1 = io.link();
  // Wait for 5 sec
  __kernel_timespec t;
  t.tv_sec = 5;
  link1.sleep(&t);
  auto rd1_awaiter = link1.read(fd1, buffer1, 128, 0);
  link1.close(fd1);
  auto rd2_awaiter = link1.read(fd2, buffer2, 256, 0);
  link1.close(fd2);
  io.submit(link1);

  int len1 = co_await rd1_awaiter;
  int len2 = co_await rd2_awaiter;

  std::cout << buffer1 << std::endl;
  std::cout << buffer2 << std::endl;
  co_return len1 + len2;
}

launch<int> coroutine_1(io_service &io) {
  auto print_file_task = print_file(io);
  co_return co_await print_file_task;
}

int main(int, char **) {
  scheduler scheduler;
  io_service io(100, 0);
  //   int buffer_size[]{32, 64, 128, 256, 512};
  //   io.create_fixed_buffer(buffer_size);
  int len = coroutine_1(io).schedule_on(&scheduler);
  std::cout << "File Length : " << len << std::endl;

  return 0;
}
