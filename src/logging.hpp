#pragma once

#include <fmt/chrono.h>
#include <fmt/core.h>

#include <chrono>
#include <source_location>
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
        case debug: return base::format("debug", ctx);
        case info: return base::format("info", ctx);
        case warning: return base::format("warning", ctx);
        case error: return base::format("error", ctx);
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
               std::source_location location)
        : level_(level),
          timestamp_(clock::now()),
          message_(std::move(message)),
          location_(location) {}

    [[nodiscard]] severity level() const {
        return level_;
    }

    [[nodiscard]] clock::time_point timestamp() const {
        return timestamp_;
    }

    [[nodiscard]] std::string_view message() const {
        return message_;
    }

    [[nodiscard]] const std::source_location& location() const {
        return location_;
    }

private:
    severity level_;
    clock::time_point timestamp_;
    std::string message_;
    std::source_location location_;
};

inline void emit(const log_record& record) {
    fmt::print(
        "{:%T} [{}] {}:{} - {}\n",
        record.timestamp(),
        record.level(),
        record.location().file_name(),
        record.location().line(),
        record.message());
}

template <severity Level, typename... Args>
class log_line {
public:
    explicit log_line(fmt::format_string<Args...> format, Args&&... args,
        std::source_location location = std::source_location::current())
    : record_(Level,
        fmt::format(format, std::forward<Args>(args)...),
        location)
    {
        emit(record_);
    }
private:
    log_record record_;
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
