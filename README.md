# SMP
This project is aimed at creating a cpp coroutine framework including io_service based on io_uring and a work stealing scheduler for scheduling the coroutine.

Coroutine are function that can suspend execution and can be resumed later. This allow us to write programs where a thread dont have to wait for a result instead it can suspend the current coroutine and start executing another one and resume it once the result is available.

* Coroutine Types
    * [`launch<T>`](#launcht)
    * [`task<T>`](#taskt)
    * [`generator<T>`](#generatort)
    * [`async<T>`](#asynct)

* Coroutine Features
    * [`delay`](#delay)
    * [`delayed<T>`](#delayedt)
    * [`timer`](#timer)
    * [`cancel`](#cancel)
    * [`schedule_on`](#scheduleon)
* IO Features
    * [`Chain Request`](#chain-request)
    * [`Batch Operation`](#batch-operation)
    * [`Fixed Buffers`](#fixed-buffers)
    * [`Provide Buffers`](#provide-buffers)

* IO Operations
    * [`openat`](#openat)
    * [`read`](#read)
    * [`write`](#write)
    * [`readv`](#readv)
    * [`writev`](#writev)
    * [`read_fixed`](#readfixed)
    * [`write_fixed`](#writefixed)
    * [`close`](#close)
    * [`accept`](#accept)
    * [`send`](#send)
    * [`recv`](#recv)
    * [`cancel`](#cancel-1)
    * [`statx`](#statx)
    * [`timeout`](#timeout)
    * [`link_timeout`](#linktimeout)
    * [`delay`](#delay)
    * [`poll`](#poll)
    * [`nop`](#nop)


# Coroutine
Coroutines in this framework use a threadpool managed by the scheduler. There can be more than one scheduler at a time running different coroutines. Coroutines are designed such that when a coroutine is resumed it resumes on the scheduler that it started on but it may not be on the same thread.
## `launch<T>`
`launch` is used when we want to start our coroutine from a regular function. It is the entry point for our coroutines. `launch` require a scheduler to start running it is passed through `schedule_on` member function. When return type of `launch` is `void` the it will have a join member function to wait for its completion if not it will have a convertion operator which will cause the thread to wait till the result is available.
```c++
#include "coroutine/launch.hpp"
#include "coroutine/scheduler/scheduler.hpp"

launch<> hello() {
  std::cout << "Hello Coroutine\n";
  co_return;
}

launch<int> add(int a, int b) { co_return a + b; }

int main(int argc, char **argv) {
  scheduler schd;

  auto cr1 = hello().schedule_on(&schd); // Run coroutine on the scheduler
  cr1.join();                            // Wait for the coroutine finish

  // When returning a value we can use convertion operator to wait and get the
  // value
  int cr2 = add(10, 20).schedule_on(&schd);
  std::cout << cr2 << std::endl;

  return 0;
}
```

## `task<T>`
A `task` is one of the basic coroutine type. When a task is created it is suspended initially and return an awaiter to the caller. To run this task we have to co_await it. When the task is co_awaited the calling coroutine is suspended and the task is resumed and the calling coroutine is set as a continuation of the task, so when the task completes it can resume the suspended coroutine.
```c++
#include "coroutine/launch.hpp"
#include "coroutine/scheduler/scheduler.hpp"
#include "coroutine/task.hpp"
#include "io/io_service.hpp"

task<int> print_file(io_service &io, const std::string &filename) {
  char buffer[128]{0};
  int dir = open(".", 0);
  int fd  = co_await io.openat(dir, filename.c_str(), 0, 0);
  int len = co_await io.read(fd, buffer, 128, 0);
  int ret = co_await io.close(fd);
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
## `generator<T>`

## `async<T>`
async coroutine start in suspended state. It can then be scheduled to run asynchronously by passing a scheduler to `schedule_on` member function or by `co_await`. When using `co_await` the scheduler used for current coroutine is used to schedule the `async`.
```c++
#include "coroutine/async.hpp"
#include "coroutine/launch.hpp"
#include "coroutine/scheduler/scheduler.hpp"
#include "io/io_service.hpp"

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

## `timer`

## `delay`

## `delayed<T>`

## `cancel`

## `schedule_on`

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
### `link_timeout`
### `cancel`
### `statx`
### `nop`
### `delay`
### `poll`