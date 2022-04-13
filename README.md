# SMP
This project is aimed at creating a cpp coroutine framework including coroutine compactable io_service based on io_uring and a work stealing scheduler for scheduling the coroutine.
# Coroutine
Coroutine are function that can suspend execution and can be resumed later. This allow us to write programs where a thread dont have to wait for a result instead it can suspend the current coroutine and start executing another one and resume it once the result is available.

Three types of coroutine currently supported are launch, task and async. Launch is the entry point for the coroutine it can be called from normal function and provide methods to wait for the result. task and async can only be called from a coroutine. task starts executing in the same thread as the calling coroutine and async allow the user to start the coroutine in another thread asynchronously.

## `task<T>`
A task is one of the basic coroutine type. When a task is created it is suspended initially and return an awaiter to the caller. To run this task we have to co_await the awaiter. When the task is co_awaited the calling coroutine is suspended and the task is resumed and the calling coroutine is set as a continuation of the task, so when the task completes it can resume the suspended coroutine.
```
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
```
## `async<T>`
async coroutine start in suspended state. It can then be scheduled to run asynchronously by passing a scheduler to `schedule_on` member function or by `co_await`. When using `co_await` the scheduler used for current coroutine is used to schedule the `async`.
```
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
```
## `launch<T>`

# Scheduler
## `scheduler`
A thread pool and a work stealing scheduler.

# io
## `io_service`
Wrapper for io_uring to support cpp coroutine. io_service have a dedicated thread for handling io and the io can be invoked from multiple thread.

## Supported features
### `Chain Request`
### `Batch Operation`
### `Fixed Buffers`
### `Provide Buffers`

## Supported io operations
### `openat`
### `read`
### `write`
### `readv`
### `writev`
### `read_fixed`
### `write_fixed`
### `close`
### `accept`
### `send`
### `recv`
### `nop`
### `timeout`
### `cancel`