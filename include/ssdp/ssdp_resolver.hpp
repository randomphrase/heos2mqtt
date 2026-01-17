#pragma once

#include "logging/logging.hpp"

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/bind_allocator.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/multicast.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/http.hpp>

#include <fmt/ostream.h>

#include <array>
#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace heos2mqtt {

namespace net = boost::asio;
namespace http = boost::beast::http;
using namespace logging;

inline const net::ip::udp::endpoint default_ssdp_endpoint(
    net::ip::make_address("239.255.255.250"), static_cast<net::ip::port_type>(1900));

class ssdp_resolver {
public:
    using udp = net::ip::udp;
    using completion_handler_type =
        net::any_completion_handler<void(boost::system::error_code, net::ip::address)>;

    static constexpr std::chrono::seconds default_timeout {3};

    ssdp_resolver(net::io_context& io, udp::endpoint endpoint = default_ssdp_endpoint)
      : strand_(net::make_strand(io))
      , socket_(io)
      , timer_(io)
      , target_endpoint_(std::move(endpoint))
    {}

    void set_outbound_interface(std::optional<net::ip::address_v4> iface) {
        outbound_interface_ = std::move(iface);
    }

    template <net::completion_token_for<void(boost::system::error_code, net::ip::address)> CompletionToken>
    auto async_resolve(std::string_view search_target,
        std::chrono::steady_clock::duration timeout,
        CompletionToken&& token) // NOLINT(cppcoreguidelines-missing-std-forward)
    {
        auto initiation = [this, search_target = std::string(search_target), timeout](auto&& handler) mutable {
          this->begin_resolve(std::move(search_target), timeout, std::forward<decltype(handler)>(handler));
        };
        return net::async_initiate<CompletionToken, void(boost::system::error_code, net::ip::address)>(initiation, token);
    }

    template <net::completion_token_for<void(boost::system::error_code, net::ip::address)> CompletionToken>
    auto async_resolve(std::string_view search_target, CompletionToken&& token) {
      return async_resolve(search_target, default_timeout, std::forward<CompletionToken>(token));
    }

private:
    template <typename Handler>
    void begin_resolve(std::string&& search_target,
                       std::chrono::steady_clock::duration timeout,
                       Handler&& handler);

    void schedule_receive();
    void handle_receive(const boost::system::error_code& ec, std::size_t bytes);
    void handle_timeout(const boost::system::error_code& ec);
    void finish(const boost::system::error_code& ec, net::ip::address address);
    [[nodiscard]] bool response_matches(std::string_view payload) const;

    net::strand<net::io_context::executor_type> strand_;
    udp::socket socket_;
    net::steady_timer timer_;
    udp::endpoint sender_;
    std::array<char, 2048> buffer_{};
    completion_handler_type handler_;
    std::chrono::steady_clock::duration timeout_{std::chrono::seconds(3)};
    std::string request_;
    std::string search_target_;
    udp::endpoint target_endpoint_;
    std::optional<net::ip::address_v4> outbound_interface_;
    bool resolving_{false};
};

template <typename Handler>
void ssdp_resolver::begin_resolve(std::string&& search_target,
                                  std::chrono::steady_clock::duration timeout,
                                  Handler&& handler) {
    completion_handler_type completion(std::forward<Handler>(handler));
    net::dispatch(
        strand_, [this, search_target = std::move(search_target), timeout,
                  completion = std::move(completion)]() mutable {
            if (resolving_) {
                auto ec = make_error_code(boost::system::errc::operation_in_progress);
                auto handler = std::move(completion);
                net::post(strand_, [handler = std::move(handler), ec]() mutable {
                    if (handler) {
                        handler(ec, net::ip::address{});
                    }
                });
                return;
            }

            resolving_ = true;
            handler_ = std::move(completion);
            timeout_ = timeout;
            search_target_ = std::move(search_target);

            request_.clear();
            request_.reserve(256);
            request_.append("M-SEARCH * HTTP/1.1\r\nHOST: ");
            auto target_addr = target_endpoint_.address();
            if (target_addr.is_v6()) {
                request_.push_back('[');
                request_.append(target_addr.to_string());
                request_.push_back(']');
            } else {
                request_.append(target_addr.to_string());
            }
            request_.push_back(':');
            request_.append(std::to_string(target_endpoint_.port()));
            request_.append("\r\nMAN: \"ssdp:discover\"\r\nMX: 2\r\nST: ");
            request_.append(search_target_);
            request_.append("\r\n\r\n");

            debug("SSDP: sending search to {}:{} (ST: {})",
                target_endpoint_.address().to_string(),
                target_endpoint_.port(),
                search_target_);

            socket_.open(target_endpoint_.protocol());
            socket_.set_option(net::socket_base::reuse_address(true));
            socket_.bind(udp::endpoint(target_endpoint_.protocol(), 0));
            if (target_endpoint_.address().is_multicast() && target_endpoint_.protocol() == udp::v4() && outbound_interface_) {
                socket_.set_option(
                    net::ip::multicast::outbound_interface(outbound_interface_->to_uint()));
            }

            socket_.async_send_to(
                net::buffer(request_), target_endpoint_,
                net::bind_executor(
                    strand_, [this](const boost::system::error_code& send_ec, std::size_t /*bytes*/) {
                        if (send_ec) {
                            finish(send_ec, {});
                            return;
                        }
                        timer_.expires_after(timeout_);
                        timer_.async_wait(net::bind_executor(
                            strand_, [this](const boost::system::error_code& timer_ec) {
                                handle_timeout(timer_ec);
                            }));
                        schedule_receive();
                    }));
        });
}

inline void ssdp_resolver::schedule_receive() {
    socket_.async_receive_from(
        net::buffer(buffer_), sender_,
        net::bind_executor(
            strand_, [this](const boost::system::error_code& ec, std::size_t bytes) {
                handle_receive(ec, bytes);
            }));
}

inline void ssdp_resolver::handle_receive(const boost::system::error_code& ec, std::size_t bytes) {
    if (ec) {
        logging::warning("SSDP: receive error: {}", ec.message());
        finish(ec, {});
        return;
    }

    std::string_view payload(buffer_.data(), bytes);
    debug("SSDP: received {} bytes from {}", bytes, fmt::streamed(sender_.address()));
    if (response_matches(payload)) {
        info("SSDP: matched response from {}", fmt::streamed(sender_.address()));
        finish({}, sender_.address());
        return;
    }
    debug("SSDP: response did not match search target");
    schedule_receive();
}

inline void ssdp_resolver::handle_timeout(const boost::system::error_code& ec) {
    if (ec == net::error::operation_aborted) {
        return;
    }
    warning("SSDP: discovery timed out");
    finish(make_error_code(net::error::timed_out), {});
}

inline void ssdp_resolver::finish(const boost::system::error_code& ec, net::ip::address address) {
    if (!resolving_) {
        return;
    }
    resolving_ = false;
    timer_.cancel();
    boost::system::error_code ignored;
    socket_.close(ignored);

    auto handler = std::move(handler_);
    handler_ = {};
    net::post(strand_, [handler = std::move(handler), ec, address]() mutable {
        if (handler) {
            handler(ec, address);
        }
    });
}

inline bool ssdp_resolver::response_matches(std::string_view payload) const {
    http::response_parser<http::string_body> parser;
    parser.eager(true);
    parser.skip(true);

    boost::system::error_code ec;
    parser.put(net::buffer(payload.data(), payload.size()), ec);
    if (ec && ec != http::error::need_more) {
        debug("SSDP: parse error: {}", ec.message());
        return false;
    }
    if (!parser.is_header_done()) {
        debug("SSDP: incomplete response headers");
        return false;
    }

    const auto& response = parser.get();
    if (response.result() != http::status::ok) {
        debug("SSDP: non-OK response {}", response.result_int());
        return false;
    }

    auto st = response.find("ST");
    if (st == response.end()) {
        debug("SSDP: missing ST header");
        return false;
    }

    if (st->value() != search_target_) {
        debug("SSDP: ST mismatch (got '{}')", st->value());
        return false;
    }
    return true;
}

}  // namespace heos2mqtt
