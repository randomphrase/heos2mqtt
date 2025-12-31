#pragma once

#include "run_until.hpp"

#include <boost/asio.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace test {

class ssdp_responder {
public:
    using udp = boost::asio::ip::udp;

    struct request {
        std::string payload;
        udp::endpoint sender;
    };

    explicit ssdp_responder(boost::asio::io_context& io,
                            udp::endpoint listen_endpoint = udp::endpoint{udp::v4(), 0})
        : io_(io),
          socket_(io) {
        boost::system::error_code ec;
        socket_.open(listen_endpoint.protocol(), ec);
        if (ec) {
            throw std::runtime_error("ssdp_responder socket open failed: " + ec.message());
        }
        socket_.set_option(boost::asio::socket_base::reuse_address(true), ec);
        socket_.bind(listen_endpoint, ec);
        if (ec) {
            throw std::runtime_error("ssdp_responder socket bind failed: " + ec.message());
        }
    }

    ~ssdp_responder() {
        close();
    }

    std::uint16_t port() const {
        return socket_.local_endpoint().port();
    }

    udp::endpoint endpoint() const {
        return socket_.local_endpoint();
    }

    bool expect_request(std::chrono::steady_clock::duration timeout,
                        std::string_view expected_substring,
                        std::string_view response) {
        last_request_.reset();
        boost::system::error_code receive_ec;
        bool received = false;

        socket_.async_receive_from(
            boost::asio::buffer(buffer_), sender_,
            [&](const boost::system::error_code& ec, std::size_t bytes) {
                receive_ec = ec;
                received = true;
                if (!ec) {
                    last_request_ = request{std::string(buffer_.data(), bytes), sender_};
                }
            });

        if (!run_until(io_, timeout, [&]() { return received; })) {
            boost::system::error_code ec;
            socket_.cancel(ec);
            return false;
        }

        if (receive_ec || !last_request_) {
            return false;
        }

        if (!expected_substring.empty() &&
            last_request_->payload.find(expected_substring) == std::string::npos) {
            return false;
        }

        if (!response.empty()) {
            return send_response(response, last_request_->sender);
        }

        return true;
    }

    const std::optional<request>& last_request() const {
        return last_request_;
    }

    void close() {
        if (!socket_.is_open()) {
            return;
        }
        boost::system::error_code ec;
        socket_.close(ec);
    }

private:
    bool send_response(std::string_view response, const udp::endpoint& target) {
        boost::system::error_code ec;
        socket_.send_to(boost::asio::buffer(response), target, 0, ec);
        return !ec;
    }

    boost::asio::io_context& io_;
    udp::socket socket_;
    std::array<char, 2048> buffer_{};
    udp::endpoint sender_;
    std::optional<request> last_request_;
};

}  // namespace test
