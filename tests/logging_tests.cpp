#include "logging/logging.hpp"

#include <catch2/catch_test_macros.hpp>

#include <iostream>
#include <sstream>
#include <string>

namespace {

class scoped_logger_stream {
public:
    scoped_logger_stream()
        : stream_(&buffer_) {
        logging::logger::instance().set_stream(stream_);
    }

    scoped_logger_stream(scoped_logger_stream &&) = delete;
    scoped_logger_stream &operator=(const scoped_logger_stream &) = delete;
    scoped_logger_stream &operator=(scoped_logger_stream &&) = delete;
    scoped_logger_stream(const scoped_logger_stream &) = delete;

    ~scoped_logger_stream() {
        logging::logger::instance().set_stream(std::clog);
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
    scoped_logger_stream capture;

    logging::info("hello {}", "world");

    auto output = capture.str();
    CHECK(output.find("hello world") != std::string::npos);
    CHECK(output.find("[INF]") != std::string::npos);
}
