#include "coroutine/async.hpp"
#include "coroutine/generator.hpp"
#include "coroutine/launch.hpp"
#include "coroutine/scheduler/scheduler.hpp"
#include "coroutine/task.hpp"
#include "io/io_service.hpp"

#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <linux/time_types.h>
#include <netinet/in.h>
#include <sys/socket.h>

using io = io_service;

char *send_buffer;
char *read_buffer;

size_t sb_len;
size_t rb_len;

int get_tcp_socket();

task<> fill_response_from_file(io *io) {
  int dfd = open(".", 0);
  int fd  = co_await io->openat(dfd, "response", 0, 0);
  sb_len  = co_await io->read_fixed(fd, send_buffer, 1024, 0, 0);
  co_await io->close(fd);
}

async<> handle_client(int fd, io *io) {

  while (true) {
    auto r = co_await io->read_fixed(fd, read_buffer, rb_len, 0, 1);
    if (r <= 0) {
      co_await io->close(fd);
      co_return;
    }
    co_await io->write_fixed(fd, send_buffer, sb_len, 0, 0);
  }

  co_return;
}

// This coroutine will suspend untill a connection is received and yield the
// connection fd.
generator<int> get_connections(io *io, int socket_fd) {
  // Get a stop_token for this coroutine.
  auto st = co_await get_stop_token();
  while (!st.stop_requested()) { // Run untill a cancel is requested.
    sockaddr_in client_addr;
    socklen_t client_length;

    // Submit an io request for accepting a connection.
    auto awaiter =
        io->accept(socket_fd, (sockaddr *)&client_addr, &client_length, 0);

    // Register a stop_callback for this coroutine.
    std::stop_callback sc(st, [&] { io->cancel(awaiter, 0); });

    // Suspend this coroutine till a connection is received and yield the
    // connection fd.
    co_yield co_await awaiter;
  }
  co_yield 0;
}

async<> server(io *io) {
  int socket_fd = get_tcp_socket();
  co_await fill_response_from_file(io);

  // Get a connection generator
  auto connections = get_connections(io, socket_fd);

  // Cancel the get_connection coroutine if this coroutine receives cancelation.
  std::stop_callback cb(co_await get_stop_token(),
                        [&] { connections.cancel(); });

  // Run untill get_connection coroutine is stoped.
  while (connections) {

    // Get a connection
    auto fd = co_await connections;
    if (fd <= 0) {
      co_return;
    }

    // Handle the client in another coroutine asynchronously
    handle_client(fd, io).schedule_on(co_await get_scheduler());
  }
  co_return;
}

launch<> start_accept(io *io) {
  auto s = server(io).schedule_on(co_await get_scheduler());
  sleep(2);
  std::cout << "Press Enter to stop\n";
  std::cin.get();
  co_await s.cancel();
}

int main(int argc, char **argv) {
  scheduler scheduler;
  io io(1000, 0);

  rb_len      = 1024;
  send_buffer = new char[1024];
  read_buffer = new char[rb_len];

  iovec vec[2];
  vec[0].iov_base = send_buffer;
  vec[0].iov_len  = 1024;
  vec[1].iov_base = read_buffer;
  vec[1].iov_len  = 1024;

  if (!io.register_buffer(vec)) {
    std::cerr << "Buffer Registeration failed\n";
  }

  start_accept(&io).schedule_on(&scheduler).join();

  return 0;
}

int get_tcp_socket() {
  int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  const int val = 1;
  int ret = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
  if (ret == -1) {
    std::cerr << "setsockopt() set_reuse_address\n";
    return -1;
  }

  sockaddr_in serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family      = AF_INET;
  serv_addr.sin_port        = htons(8080);
  serv_addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(socket_fd, (sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    std::cerr << "Error binding socket\n";
    return -1;
  }
  if (listen(socket_fd, 240) < 0) {
    std::cerr << "Error listening\n";
    return -1;
  }
  return socket_fd;
}