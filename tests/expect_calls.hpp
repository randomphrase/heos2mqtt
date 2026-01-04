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

    state(std::source_location location, unsigned expected)
      : location_(std::move(location)), expected_(expected)
    {}

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
  call_count_checker(unsigned expected,
    std::source_location location = std::source_location::current())
    : state_{std::make_shared<state>(std::move(location), expected)}
  {}
  call_count_checker(const call_count_checker&) = default;
  call_count_checker(call_count_checker&&) = default;

  template <typename ... Args>
  void operator() (Args&& ...) const {
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
auto expect_calls(unsigned expected, callable_t&& callable, std::source_location location = std::source_location::current()) {
  return decorated_callable<call_count_checker, callable_t>{{expected, std::move(location)}, std::forward<callable_t>(callable)};
}

}
