#pragma once

#include <boost/asio/io_context.hpp>

#include <chrono>
#include <thread>

namespace test {

using namespace std::chrono_literals;

using clock_type = std::chrono::steady_clock;

inline constexpr clock_type::duration default_timeout = 5s;

// When a test calls run_remaining it should time out quickly - the
// assumption being that there is not any sigificant work left to do.
inline constexpr clock_type::duration remaining_timeout = 500ms;

[[noreturn]] inline void timeout_error() {
    // TODO: log a test failure
    throw std::runtime_error("run_until: timeout expired");
}

template <std::predicate Predicate>
void run_until(boost::asio::io_context& io,
    Predicate&& predicate,
    clock_type::duration timeout = default_timeout)
{
    auto deadline = clock_type::now() + timeout;
    while (clock_type::now() < deadline) {
        if (std::forward<Predicate>(predicate)()) {
            return;
        }
        if (io.poll_one() == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    if (std::forward<Predicate>(predicate)()) {
        return;
    }
    timeout_error();
}

inline void run_for(boost::asio::io_context& io, clock_type::duration duration) {
    const auto end = clock_type::now() + duration;
    run_until(io, [&]() { return clock_type::now() >= end; }, duration);
}

inline void run_remaining(boost::asio::io_context& io) {
    run_for(io, remaining_timeout);
}

}  // namespace test
