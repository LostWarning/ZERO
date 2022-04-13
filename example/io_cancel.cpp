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

async<> wait_for_connection(auto &awaiter) {
  std::cout << "Waiting for connection\n";
  co_await awaiter;
  std::cerr << "Request Cancelled\n";
  co_return;
}

async<> loop(io *io, int socket_fd) {
  while (true) {
    sockaddr_in client_addr;
    socklen_t client_length;
    auto client_awaiter =
        io->accept(socket_fd, (sockaddr *)&client_addr, &client_length, 0);
    wait_for_connection(client_awaiter).schedule_on(co_await get_scheduler());
    std::cerr << "Sleeping for 5 sec\n";
    sleep(5);
    co_await io->cancel(client_awaiter, 0);
  }
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

  co_await loop(io, socket_fd);
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