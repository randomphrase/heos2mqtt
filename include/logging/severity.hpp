#pragma once

#include "logging/fwd.hpp"

#include <fmt/core.h>

namespace logging {

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

    auto format(logging::severity s, format_context& ctx) const
        -> format_context::iterator;
};

}  // namespace fmt
