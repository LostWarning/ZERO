#include "coroutine/async.hpp"
#include "coroutine/launch.hpp"
#include "coroutine/scheduler/scheduler.hpp"
#include "coroutine/task.hpp"
#include "io/io_service.hpp"
#include <malloc.h>

#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <linux/time_types.h>
#include <netinet/in.h>
#include <sys/socket.h>

using io = io_service;

async<int> loop(io *io, int socket_fd) {
  auto st = co_await get_stop_token();
  while (!st.stop_requested()) {
    sockaddr_in client_addr;
    socklen_t client_length;
    auto client_awaiter =
        io->accept(socket_fd, (sockaddr *)&client_addr, &client_length, 0);
    std::stop_callback sr(st, [&] {
      std::cerr << "Stop Requested\n";
      return io->cancel(client_awaiter, 0);
    });
    std::cerr << "Waiting for request\n";
    int fd = co_await client_awaiter;
    if (fd <= 0) {
      std::cerr << "IO Request cancelled\n";
    }
    std::cerr << "Wait 5 sec before exiting\n";
    sleep(5);
  }
  co_return 1;
}

async<void> waiter(auto &awaiter) {
  std::cerr << "Waiter() Enter\n";
  co_await awaiter;
  std::cerr << "Waiter() Exit\n";
}

async<void> cancelable(io *io, int socket_fd) {
  auto loop1 = loop(io, socket_fd).schedule_on(co_await get_scheduler());
  auto w     = waiter(loop1).schedule_on(co_await get_scheduler());
  std::cerr << "Sleepint for 5 sec\n";
  sleep(5);
  std::cerr << "Requesting cancel\n";
  co_await loop1.cancel();
  std::cerr << "loop canceled\n";
  co_await w;
}

launch<> start_accept(io *io) {

  int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  const int val = 1;
  int ret = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
  if (ret == -1) {
    std::cerr << "setsockopt() set_reuse_address\n";
    co_return;
  }

  sockaddr_in serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family      = AF_INET;
  serv_addr.sin_port        = htons(8080);
  serv_addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(socket_fd, (sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    std::cerr << "Error binding socket\n";
    co_return;
  }
  if (listen(socket_fd, 240) < 0) {
    std::cerr << "Error listening\n";
    co_return;
  }

  co_await cancelable(io, socket_fd);
  co_return;
}

int main(int argc, char **argv) {
  scheduler scheduler;
  io io(1000, 0);
  if (argc == 2) {
    scheduler.spawn_workers(atoi(argv[1]));
  }

  start_accept(&io).schedule_on(&scheduler).join();

  return 0;
}