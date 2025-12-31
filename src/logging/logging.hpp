#pragma once

#include <fmt/core.h>

#include <chrono>
#include <cstdint>
#include <source_location>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

namespace logging {

using clock = std::chrono::system_clock;

enum class severity : uint8_t {
    debug,
    info,
    warning,
    error
};

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
    log_record(severity level,
               std::string message,
               std::source_location location);

    [[nodiscard]] severity level() const;
    [[nodiscard]] clock::time_point timestamp() const;
    [[nodiscard]] std::string_view message() const;
    [[nodiscard]] const std::source_location& location() const;

private:
    severity level_;
    clock::time_point timestamp_;
    std::string message_;
    std::source_location location_;
};

class logger {
public:
    static logger& instance();
    void set_stream(std::ostream& stream);
    void emit(const log_record& record);

private:
    logger();

    std::ostream* stream_{};
};

template <severity Level, typename... Args>
class log_line {
public:
    explicit log_line(fmt::format_string<Args...> format, Args&&... args,
        std::source_location location = std::source_location::current())
    {
        log_record record {
            Level,
            fmt::format(format, std::forward<Args>(args)...),
            location};
        logger::instance().emit(record);
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
