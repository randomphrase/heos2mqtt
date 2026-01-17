#pragma once

#include "logging/fwd.hpp"
#include "logging/severity.hpp"
#include "logging/log_record.hpp"
#include "logging/log_destination.hpp"

#include <source_location>

namespace logging {

class logger {
public:
    static logger& get_instance(const std::source_location& location);

    static logger& get_default();

    using log_destination_ptr = std::shared_ptr<log_destination>;

    logger(severity min_level, log_destination_ptr&& default_dest,
        std::initializer_list<std::pair<severity, log_destination_ptr>> level_dests = {});
    logger(const logger&) = default;
    logger& operator=(const logger&) noexcept = default;
    logger(logger&&) noexcept = default;
    logger& operator=(logger&&) noexcept = default;
    ~logger() = default;

    template <severity Level, typename... Args>
    static void log(std::integral_constant<severity, Level> level, const std::source_location& location, fmt::format_string<Args...> format, Args&&... args) {
        auto* dest = get_instance(location).get_destination_for_level(level);
        if (!dest) {
            return;
        }
        buffer_ = fmt::format(format, std::forward<Args>(args)...);
        log_record record {level, buffer_, location};
        dest->emit(record);
    }

    template <severity Level>
    [[nodiscard]] log_destination* get_destination_for_level(std::integral_constant<severity,Level> /*unused*/) const {
        return level_destinations_[static_cast<std::size_t>(Level)];
    }

    void set_min_level(severity min_level);

private:
    std::array<log_destination*, max_enum_value(severity{}) + 1> level_destinations_{};
    std::vector<log_destination_ptr> destinations_;
    thread_local static std::string buffer_;
};

}  // namespace logging
