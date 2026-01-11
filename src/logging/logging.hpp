#pragma once

#include <fmt/core.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <source_location>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace logging {

using clock = std::chrono::system_clock;

enum class severity : uint8_t {
    debug,
    info,
    warning,
    error
};
constexpr size_t max_enum_value(severity /*unused*/) {
    return static_cast<size_t>(severity::error);
}

}
namespace fmt {
template <> struct formatter<logging::severity>: formatter<string_view> {
    using base = formatter<string_view>;

    constexpr auto format(logging::severity s, format_context& ctx) const
    -> format_context::iterator {
        using enum logging::severity;
        switch (s) {
        case debug: return base::format("DBG", ctx);
        case info: return base::format("INF", ctx);
        case warning: return base::format("WRN", ctx);
        case error: return base::format("ERR", ctx);
        }
        __builtin_unreachable();
        //std::unreachable();
    }
};

}
namespace logging {

class log_record {
public:
    log_record(
        severity level,
        std::string_view message,
        std::source_location location);

    [[nodiscard]] severity level() const;
    [[nodiscard]] clock::time_point timestamp() const;
    [[nodiscard]] std::string_view message() const;
    [[nodiscard]] const std::source_location& location() const;
    [[nodiscard]] std::string_view source_file() const;

private:
    severity level_;
    clock::time_point timestamp_;
    std::string_view message_;
    std::source_location location_;
};

class log_destination {
public:
    log_destination() = default;

    log_destination(const log_destination&) = default;
    log_destination& operator=(const log_destination&) = default;
    log_destination(log_destination&&) = default;
    log_destination& operator=(log_destination&&) = default;

    virtual ~log_destination();
    virtual void emit(const log_record& record) = 0;
};

class log_destination_ostream final : public log_destination {
public:
    explicit log_destination_ostream(std::ostream& stream);

    log_destination_ostream(const log_destination_ostream&) = delete;
    log_destination_ostream& operator=(const log_destination_ostream&) = delete;
    log_destination_ostream(log_destination_ostream&&) = delete;
    log_destination_ostream& operator=(log_destination_ostream&&) = delete;
    ~log_destination_ostream() override = default;

    void emit(const log_record& record) override;
private:
    std::ostream& stream_;
};

class logger {
public:
    static logger& get_instance(const std::source_location& location);

    static logger& get_default();

    using log_destination_ptr = std::shared_ptr<log_destination>;

    logger(severity min_level, log_destination_ptr&& default_dest,
        std::initializer_list<std::pair<severity, log_destination_ptr>> level_dests = {});
    logger(const logger&) = default;
    logger& operator=(const logger&) = default;
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

private:
    std::array<log_destination*, max_enum_value(severity{}) + 1> level_destinations_{};
    std::vector<log_destination_ptr> destinations_;
    thread_local static std::string buffer_;
};

template <severity Level, typename... Args>
class log_line {
public:
    explicit log_line(fmt::format_string<Args...> format, Args&&... args,
        std::source_location location = std::source_location::current())
    {
        logger::log(std::integral_constant<severity, Level>{}, location, format, std::forward<Args>(args)...);
    }
};

template <typename... Args>
struct debug : log_line<severity::debug, Args...> {
    using log_line<severity::debug, Args...>::log_line;
};

template <typename... Args>
debug(fmt::format_string<Args...>, Args&&...) -> debug<Args...>;

template <typename... Args>
struct info : log_line<severity::info, Args...> {
    using log_line<severity::info, Args...>::log_line;
};

template <typename... Args>
info(fmt::format_string<Args...>, Args&&...) -> info<Args...>;

template <typename... Args>
struct warning : log_line<severity::warning, Args...> {
    using log_line<severity::warning, Args...>::log_line;
};

template <typename... Args>
warning(fmt::format_string<Args...>, Args&&...) -> warning<Args...>;

template <typename... Args>
struct error : log_line<severity::error, Args...> {
    using log_line<severity::error, Args...>::log_line;
};

template <typename... Args>
error(fmt::format_string<Args...>, Args&&...) -> error<Args...>;

}  // namespace logging
