#ifndef __HTTP_HTTP_HEADER_HPP__
#define __HTTP_HTTP_HEADER_HPP__

#include <optional>
#include <string_view>
#include <vector>

namespace http {

enum class HTTP_METHOD { GET, POST, PUT, DELETE, HEAD, PATCH };

consteval size_t get_buffer_alloc_size(const size_t size) {
  if (size < 1024) {
    return 1024;
  }
  return size % 1024 ? (((size / 1024) + 1) * 1024) : size;
}

template <size_t BUFFER_SIZE>
struct http_header {
  struct header_item {
    char *m_key;
    char *m_value;
  };
  char m_data[get_buffer_alloc_size(BUFFER_SIZE)]{0};
  int m_length = 0;

  // Request line
  char *m_method               = nullptr;
  char *m_raw_url              = nullptr;
  char *m_http_version         = nullptr;
  bool m_header_fetch_complete = false;

  std::vector<header_item> m_headers;

  bool parse() {
    int pos = 0;
    do {
      pos = get_next_header(m_length);
      if (pos == 0) {
        parse_request_line();
        return true;
      } else if (pos == -1) {
        return false;
      }

      parser_header_line(m_length);
      m_length = pos;
    } while (pos);
    return true;
  }

  size_t get_buffer_size() noexcept {
    return get_buffer_alloc_size(BUFFER_SIZE) - m_length;
  }

  std::optional<std::string_view> get_header(const std::string_view &key) {
    for (const header_item &iter : m_headers) {
      if (iter.m_key == key) {
        return iter.m_value;
      }
    }
    return std::nullopt;
  }

private:
  size_t get_next_header(size_t curr_pos) {
    for (size_t i = curr_pos; i < BUFFER_SIZE && m_data[i] != 0; ++i) {
      if (m_data[i] == '\r' && m_data[i + 1] == '\n') {
        m_data[i] = m_data[i + 1] = 0;
        if (i == curr_pos) {
          return 0;
        }
        return i + 2;
      }
    }
    return -1;
  }

  void parse_request_line() {
    m_method = m_data;
    std::string_view request_line(m_data);

    size_t pos;
    if ((pos = request_line.find(' ', 0)) != std::string_view::npos) {
      m_data[pos] = '\0';
      m_raw_url   = m_data + pos + 1;
    }

    if ((pos = request_line.find(' ', pos)) != std::string_view::npos) {
      m_data[pos]    = '\0';
      m_http_version = m_data + pos + 1;
    }
  }

  void parser_header_line(int curr_pos) {
    std::string_view h(m_data + curr_pos);
    size_t h_index             = h.find(':');
    m_data[curr_pos + h_index] = 0;
    for (int i = curr_pos; m_data[i] != 0; ++i) {
      m_data[i] = tolower(m_data[i]);
    }
    m_headers.push_back({m_data + curr_pos, m_data + curr_pos + h_index + 1});
  }
};

} // namespace http
#endif