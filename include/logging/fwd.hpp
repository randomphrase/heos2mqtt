#pragma once

#include <chrono>

namespace logging {

using clock = std::chrono::system_clock;

enum class severity : uint8_t;

class log_record;
class log_destination;
class log_destination_ostream;
class logger;

}
