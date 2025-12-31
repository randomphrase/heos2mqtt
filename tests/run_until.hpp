#pragma once

#include <boost/asio/io_context.hpp>

#include <chrono>
#include <thread>

namespace test {

using clock_type = std::chrono::steady_clock;

template <typename Predicate>
bool run_until(boost::asio::io_context& io,
               clock_type::duration timeout,
               Predicate predicate) {
    auto deadline = clock_type::now() + timeout;
    while (clock_type::now() < deadline) {
        if (predicate()) {
            return true;
        }
        if (io.poll_one() == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    return predicate();
}

}  // namespace test
