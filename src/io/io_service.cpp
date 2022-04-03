#include "io_service.hpp"

thread_local unsigned int io_service::m_thread_id                = 0;
thread_local uring_allocator *io_service::m_uring_data_allocator = nullptr;