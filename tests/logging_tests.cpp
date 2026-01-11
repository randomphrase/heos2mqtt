#include "logging/logging.hpp"

#include <catch2/catch_test_macros.hpp>

#include <iostream>
#include <memory>
#include <sstream>
#include <string>

namespace {

class scoped_logger_override {
public:
    explicit scoped_logger_override(logging::logger replacement)
      : saved_(logging::logger::get_default())
    {
        logging::logger::get_default() = std::move(replacement);
    }
    scoped_logger_override(const scoped_logger_override &) = delete;
    scoped_logger_override(scoped_logger_override &&) = delete;
    scoped_logger_override &operator=(const scoped_logger_override &) = delete;
    scoped_logger_override &operator=(scoped_logger_override &&) = delete;

    ~scoped_logger_override() {
        logging::logger::get_default() = saved_;
    }

private:
    logging::logger saved_;
};

class log_capture {
public:
    log_capture()
        : stream_(&buffer_) {}

    std::shared_ptr<logging::log_destination_ostream> destination() {
        return std::make_shared<logging::log_destination_ostream>(stream_);
    }

    std::string str() const {
        return buffer_.str();
    }

private:
    std::stringbuf buffer_;
    std::ostream stream_;
};

}  // namespace

TEST_CASE("logging emits formatted messages", "[logging]") {
    log_capture capture;
    auto destination = capture.destination();
    scoped_logger_override guard(logging::logger(logging::severity::debug, std::move(destination)));

    logging::info("hello {}", "world");

    auto output = capture.str();
    CHECK(output.find("hello world") != std::string::npos);
    CHECK(output.find("[INF]") != std::string::npos);
}

TEST_CASE("logging routes by severity destination", "[logging]") {
    log_capture default_capture;
    log_capture warning_capture;
    log_capture error_capture;

    auto default_dest = default_capture.destination();
    auto warning_dest = warning_capture.destination();
    auto error_dest = error_capture.destination();

    scoped_logger_override guard(logging::logger(
        logging::severity::debug,
        std::move(default_dest),
        {
            {logging::severity::warning, warning_dest},
            {logging::severity::error, error_dest},
        }));

    logging::info("info {}", 1);
    logging::warning("warn {}", 2);
    logging::error("err {}", 3);

    auto info_output = default_capture.str();
    auto warning_output = warning_capture.str();
    auto error_output = error_capture.str();

    CHECK(info_output.find("info 1") != std::string::npos);
    CHECK(info_output.find("warn 2") == std::string::npos);
    CHECK(info_output.find("err 3") == std::string::npos);

    CHECK(warning_output.find("warn 2") != std::string::npos);
    CHECK(warning_output.find("err 3") == std::string::npos);

    CHECK(error_output.find("err 3") != std::string::npos);
}
