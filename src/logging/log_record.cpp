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

severity log_record::level() const {
    return level_;
}

clock::time_point log_record::timestamp() const {
    return timestamp_;
}

std::string_view log_record::message() const {
    return message_;
}

const std::source_location& log_record::location() const {
    return location_;
}

std::string_view log_record::source_file() const {
    std::string_view file = location_.file_name();
    if (auto pos = file.find_last_of(std::filesystem::path::preferred_separator); pos != std::string_view::npos) {
        file.remove_prefix(pos + 1);
    }
    return file;
}

}
