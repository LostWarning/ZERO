#include "coroutine/launch.hpp"
#include "coroutine/scheduler/scheduler.hpp"
#include "io/io_service.hpp"

#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

launch<> print_file(io_service &io, const std::string &filename) {
  int dir = open(".", 0);
  int fd  = co_await io.openat(dir, filename.c_str(), 0, 0);
  char buffer[8];

  int offset = 0, len = 0;
  while ((len = co_await io.read(fd, buffer, 7, offset)) > 0) {
    buffer[len] = '\0';
    std::cerr << buffer;
    offset += len;
  }
}

int main(int, char **) {
  scheduler scheduler;
  io_service io(100, 0);

  print_file(io, "log").schedule_on(&scheduler).join();

  return 0;
}