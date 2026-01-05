#include "logging/logging.hpp"

#include <fmt/chrono.h>
#include <fmt/core.h>

#include <iostream>
#include <iterator>
#include <filesystem>

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
        "{:%T} [{}] {}:{} - {}\n",
        record.timestamp(),
        record.level(),
        record.source_file(),
        record.location().line(),
        record.message());
}

log_record::log_record(severity level,
                       std::string_view message,
                       std::source_location location)
  : level_(level)
  , timestamp_(clock::now())
  , message_(message)
  , location_(location)
{}

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

std::string_view log_record::source_file() const {
    std::string_view file = location_.file_name();
    if (auto pos = file.find_last_of(std::filesystem::path::preferred_separator); pos != std::string_view::npos) {
        file.remove_prefix(pos + 1);
    }
    return file;
}

thread_local std::string logger::buffer_ = [] {
    std::string buf;
    buf.reserve(1024);
    return buf;
}();

logger::logger(severity min_level,
    log_destination_ptr&& default_dest,
    std::initializer_list<std::pair<severity, log_destination_ptr>> level_dests)
{
    destinations_.push_back(std::move(default_dest));
    for (std::size_t i = 0; i <= max_enum_value(severity{}); ++i) {
        auto level = static_cast<severity>(i);
        if (level >= min_level) {
            level_destinations_[i] = destinations_.back().get();
        }
    }

    for (auto&& [level, dest] : level_dests) {
        destinations_.push_back(std::move(dest));
        level_destinations_[static_cast<std::size_t>(level)] = destinations_.back().get();
    }
}

logger& logger::get_default() {
    static logger instance{
        severity::info,
        std::make_shared<log_destination_ostream>(std::clog),
        {
            {severity::error, std::make_shared<log_destination_ostream>(std::cerr)},
        }};
    return instance;
}

logger& logger::get_instance(const std::source_location& /*location*/) {
    // TODO use location to create per-module loggers
    return get_default();
}

}  // namespace logging
