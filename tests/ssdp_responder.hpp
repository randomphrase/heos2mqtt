#pragma once

#include "logging/logging.hpp"

#include "run_until.hpp"

#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/udp.hpp>

#include <fmt/ostream.h>

#include <chrono>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace test {

using namespace logging;

class ssdp_responder {
public:
    using udp = boost::asio::ip::udp;

    struct request {
        std::string payload_;
        udp::endpoint sender_;
    };

    explicit ssdp_responder(boost::asio::io_context& io,
        const udp::endpoint& listen_endpoint = {boost::asio::ip::address_v4::loopback(), 0})
    : io_{io}
    , socket_{io}
    {
        socket_.open(listen_endpoint.protocol());
        socket_.set_option(boost::asio::socket_base::reuse_address(true));
        socket_.bind(listen_endpoint);
        info("SSDP responder listening on {}", fmt::streamed(socket_.local_endpoint()));
    }

    // [[nodiscard]] std::uint16_t port() const {
    //     return socket_.local_endpoint().port();
    // }

    [[nodiscard]] udp::endpoint endpoint() const {
        return socket_.local_endpoint();
    }

    request expect_request(clock_type::duration timeout = default_timeout) {
        std::optional<request> received;

        socket_.async_receive_from(
            boost::asio::buffer(buffer_), sender_,
            [&](const boost::system::error_code& ec, std::size_t bytes) {
                if (ec) {
                    if (ec != boost::asio::error::operation_aborted) {
                        throw std::system_error(ec);
                    }
                    return;
                }
                // TODO check for a complete response - we assume it
                // will fit into a single UDP packet
                // TODO basic sanity check on the response - HTTP OK for example
                received.emplace(std::string(buffer_.data(), bytes), sender_);
            });

        test::run_until(io_, [&]() {
            return received.has_value();
        }, timeout);

        return *received;
    }

    void close() {
        if (!socket_.is_open()) {
            return;
        }
        boost::system::error_code ec;
        socket_.close(ec);
    }

    void send_response(std::string_view response, const udp::endpoint& target) {
        socket_.send_to(boost::asio::buffer(response), target, 0);
    }

private:
    boost::asio::io_context& io_; // TODO: remove, only needed to call run_until
    udp::socket socket_;
    udp::endpoint sender_;
    std::array<char, 2048> buffer_{};
};

}  // namespace test
