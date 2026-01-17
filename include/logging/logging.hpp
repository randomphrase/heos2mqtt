#pragma once

#include "logging/logger.hpp"

#include <type_traits>

namespace logging {

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
