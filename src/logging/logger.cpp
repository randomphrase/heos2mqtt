#include "logging/logger.hpp"

#include <fmt/chrono.h>
#include <fmt/core.h>

#include <iostream>
#include <iterator>

namespace logging {

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
    for (auto& dest : level_destinations_) {
        const auto level = static_cast<severity>(
            std::distance(level_destinations_.begin(), &dest));
        if (level < min_level) {
            continue;
        }
        dest = destinations_.back().get();
    }

    for (auto&& [level, dest] : level_dests) {
        destinations_.push_back(dest);
        level_destinations_.at(static_cast<std::size_t>(level)) = destinations_.back().get();
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
