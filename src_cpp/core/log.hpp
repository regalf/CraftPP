#pragma once

#include <string_view>

namespace craftpp {

// Minimal stderr logger for M0. Replaced by a proper sink in later milestones.
void log_info(std::string_view msg);
void log_error(std::string_view msg);

}  // namespace craftpp
