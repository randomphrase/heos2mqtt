#include "logging/logging.hpp"

#include <fmt/chrono.h>
#include <fmt/core.h>

#include <iostream>
#include <iterator>

namespace logging {

log_record::log_record(severity level,
                       std::string message,
                       std::source_location location)
    : level_(level),
      timestamp_(clock::now()),
      message_(std::move(message)),
      location_(location) {}

severity log_record::level() const {
    return level_;
}

clock::time_point log_record::timestamp() const {
    return timestamp_;
}

std::string_view log_record::message() const {
    return message_;
}

const std::source_location& log_record::location() const {
    return location_;
}

logger::logger()
    : stream_(&std::clog) {}

logger& logger::instance() {
    static logger instance;
    return instance;
}

void logger::set_stream(std::ostream& stream) {
    stream_ = &stream;
}

void logger::emit(const log_record& record) {
    // use ostreambuf iterator to bypass the ostreams formatting,
    // which is slow and redundant
    fmt::format_to(
        std::ostreambuf_iterator<char>(*stream_),
        "{:%T} [{}] {}:{} - {}\n",
        record.timestamp(),
        record.level(),
        record.location().file_name(),
        record.location().line(),
        record.message());
}

}  // namespace logging
