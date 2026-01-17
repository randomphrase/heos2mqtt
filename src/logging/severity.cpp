#include "logging/severity.hpp"

namespace fmt {

auto formatter<logging::severity>::format(logging::severity s, format_context& ctx) const
    -> format_context::iterator {
    using enum logging::severity;
    switch (s) {
    case debug: return base::format("DBG", ctx);
    case info: return base::format("INF", ctx);
    case warning: return base::format("WRN", ctx);
    case error: return base::format("ERR", ctx);
    }
    __builtin_unreachable();
}

}  // namespace fmt
