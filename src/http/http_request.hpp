#ifndef __HTTP_HTTP_REQUEST_HPP__
#define __HTTP_HTTP_REQUEST_HPP__

#include "http_header.hpp"

namespace http {

template <size_t BUFFER_SIZE>
struct http_request {
  unsigned int m_handle;
  http_header<BUFFER_SIZE> m_header;
};

} // namespace http

#endif