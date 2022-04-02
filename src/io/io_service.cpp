#include "io_service.hpp"

thread_local unsigned int io_service::m_thread_id = 0;