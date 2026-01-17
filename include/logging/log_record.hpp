#pragma once

#include "logging/fwd.hpp"
#include "logging/severity.hpp"

#include <source_location>

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

}
