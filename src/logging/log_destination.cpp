#include "logging/log_destination.hpp"
#include "logging/log_record.hpp"

#include <fmt/chrono.h>

namespace logging {

log_destination::~log_destination() = default;

log_destination_ostream::log_destination_ostream(std::ostream& stream)
: stream_(stream)
{}

void log_destination_ostream::emit(const log_record& record) {
    // use ostreambuf iterator to bypass the ostreams formatting,
    // which is slow and redundant
    fmt::format_to(
        std::ostreambuf_iterator<char>(stream_),
        "{:%T} {} {}:{} - {}\n",
        record.timestamp(),
        record.level(),
        record.source_file(),
        record.location().line(),
        record.message());
}

}
