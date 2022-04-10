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

char *send_buffer;
size_t send_buffer_len;

char *read_buffer;

task<> fill_response_from_file(io *io) {
  int dfd         = open(".", 0);
  int fd          = co_await io->openat(dfd, "response", 0, 0);
  send_buffer_len = co_await io->read_fixed(fd, send_buffer, 1024, 0, 0);
  co_await io->close(fd);
}

async<> handle_client(int fd, io *io) {

  while (true) {
    auto r = co_await io->recv(fd, read_buffer, 8192, 0);
    if (r <= 0) {
      co_await io->close(fd);
      co_return;
    }
    co_await io->send(fd, send_buffer, send_buffer_len, 0);
  }

  co_return;
}

async<> loop(io *io, int socket_fd) {
  while (true) {
    sockaddr_in client_addr;
    socklen_t client_length;
    int client_fd = co_await io->accept(socket_fd, (sockaddr *)&client_addr,
                                        &client_length, 0);
    handle_client(client_fd, io).schedule_on(co_await get_scheduler());
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
  co_await fill_response_from_file(io);
  std::cout << send_buffer << std::endl;
  co_await loop(io, socket_fd);
  co_return;
}

int main(int argc, char **argv) {
  scheduler scheduler;
  io io(1000, 0);
  if (argc == 2) {
    scheduler.spawn_workers(atoi(argv[1]));
  }

  send_buffer = new char[1024];
  read_buffer = new char[8192];

  iovec vec[2];
  vec[0].iov_base = send_buffer;
  vec[0].iov_len  = 1024;
  vec[1].iov_base = read_buffer;
  vec[1].iov_len  = 8192;

  if (!io.register_buffer(vec)) {
    std::cerr << "Buffer Registeration failed\n";
  }

  start_accept(&io).schedule_on(&scheduler).join();

  return 0;
}