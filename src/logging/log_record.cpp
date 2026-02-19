#include "logging/log_record.hpp"

#include <filesystem>

namespace logging {

log_record::log_record(severity level,
                       std::string_view message,
                       std::source_location location)
  : level_(level)
  , timestamp_(clock::now())
  , message_(message)
  , location_(location)
{}

auto log_record::level() const -> severity {
    return level_;
}

auto log_record::timestamp() const -> clock::time_point {
    return timestamp_;
}

auto log_record::message() const -> std::string_view {
    return message_;
}

auto log_record::location() const -> const std::source_location& {
    return location_;
}

auto log_record::source_file() const -> std::string_view {
    std::string_view file = location_.file_name();
    if (auto pos = file.find_last_of(std::filesystem::path::preferred_separator); pos != std::string_view::npos) {
        file.remove_prefix(pos + 1);
    }
    return file;
}

}
