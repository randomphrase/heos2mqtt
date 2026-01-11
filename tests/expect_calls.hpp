#pragma once

#include <catch2/catch_test_macros.hpp>  // TODO somehow include only AssertionHandler
#include <fmt/format.h>

#include <memory>
#include <source_location>

namespace test {

class call_count_checker {

    struct state {
        std::source_location location_;
        unsigned expected_;
        unsigned calls_{0};

        state(const std::source_location &location, unsigned expected)
          : location_(location)
          , expected_(expected)
        {}

        state(const state &) = delete;
        state(state &&) = delete;
        state &operator=(const state &) = delete;
        state &operator=(state &&) = delete;

        ~state() {
            if (calls_ != expected_) {
                Catch::AssertionHandler handler(
                    "EXPECT_CALLS",
                    Catch::SourceLineInfo{location_.file_name(),
                                  static_cast<std::size_t>(location_.line())},
                    Catch::StringRef{},
                    Catch::ResultDisposition::ContinueOnFailure);
                handler.handleMessage(
                    Catch::ResultWas::ExpressionFailed,
                    fmt::format("expected {} calls but got {}", expected_, calls_));
                handler.complete();
            }
        }
    };
    std::shared_ptr<state> state_;

public:
    call_count_checker(unsigned expected, const std::source_location& location = std::source_location::current())
    : state_{std::make_shared<state>(location, expected)}
    {}

    call_count_checker(const call_count_checker&) = default;
    call_count_checker(call_count_checker&&) = default;
    call_count_checker &operator=(const call_count_checker &) = delete;
    call_count_checker &operator=(call_count_checker &&) = delete;
    ~call_count_checker() = default;

    template <typename ... Args>
    void operator() (Args&& ... /*unused*/) const {
        state_->calls_ += 1;
    }
};

template <typename decorator_t, typename callable_t>
struct decorated_callable {
    decorator_t decorator_;
    callable_t callable_;

    template <typename ... args_t>
    auto operator() (args_t&& ... args) {
        decorator_(std::forward<args_t>(args)...);
        return callable_(std::forward<args_t>(args)...);
    }
};

template <typename callable_t>
auto expect_calls(unsigned expected, callable_t&& callable, const std::source_location& location = std::source_location::current()) {
    return decorated_callable<call_count_checker, callable_t>{
        {expected, location},
        std::forward<callable_t>(callable)};
}

}
