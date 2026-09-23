#include "core/log.hpp"

#include <cstdio>

namespace craftpp {

void log_info(std::string_view msg) { std::fprintf(stderr, "[info] %.*s\n", static_cast<int>(msg.size()), msg.data()); }

void log_error(std::string_view msg) {
  std::fprintf(stderr, "[error] %.*s\n", static_cast<int>(msg.size()), msg.data());
}

}  // namespace craftpp
