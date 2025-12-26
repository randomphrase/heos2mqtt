#pragma once

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/bind_allocator.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/multicast.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>

#include <array>
#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace heos2mqtt {

namespace net = boost::asio;

inline const net::ip::udp::endpoint default_ssdp_endpoint(
    net::ip::make_address("239.255.255.250"), static_cast<net::ip::port_type>(1900));

class ssdp_resolver {
public:
    using udp = net::ip::udp;
    using completion_handler_type =
        net::any_completion_handler<void(boost::system::error_code, udp::endpoint)>;

    ssdp_resolver(net::io_context& io, udp::endpoint endpoint = default_ssdp_endpoint)
        : strand_(net::make_strand(io)),
          socket_(io),
          timer_(io),
          target_endpoint_(std::move(endpoint)) {}

    void set_outbound_interface(std::optional<net::ip::address_v4> iface) {
        outbound_interface_ = std::move(iface);
    }

    template <typename CompletionToken>
    auto async_resolve(std::string search_target,
                       std::chrono::steady_clock::duration timeout,
                       CompletionToken&& token);

    template <typename CompletionToken>
    auto async_resolve(std::string search_target, CompletionToken&& token) {
        return async_resolve(std::move(search_target), std::chrono::seconds(3),
                             std::forward<CompletionToken>(token));
    }

private:
    template <typename Handler>
    void begin_resolve(std::string search_target,
                       std::chrono::steady_clock::duration timeout,
                       Handler&& handler);

    void schedule_receive();
    void handle_receive(const boost::system::error_code& ec, std::size_t bytes);
    void handle_timeout(const boost::system::error_code& ec);
    void finish(const boost::system::error_code& ec, udp::endpoint endpoint);

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

template <typename CompletionToken>
auto ssdp_resolver::async_resolve(std::string search_target,
                                  std::chrono::steady_clock::duration timeout,
                                  CompletionToken&& token) {
    return net::async_initiate<CompletionToken, void(boost::system::error_code, udp::endpoint)>(
        [this, search_target = std::move(search_target), timeout](auto&& handler) mutable {
            this->begin_resolve(std::move(search_target), timeout, std::forward<decltype(handler)>(handler));
        },
        token);
}

template <typename Handler>
void ssdp_resolver::begin_resolve(std::string search_target,
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
                        handler(ec, udp::endpoint{});
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

            boost::system::error_code ec;
            socket_.open(target_endpoint_.protocol(), ec);
            if (!ec) {
                socket_.set_option(net::socket_base::reuse_address(true), ec);
            }
            if (!ec) {
                socket_.bind(udp::endpoint(target_endpoint_.protocol(), 0), ec);
            }
            if (!ec && target_endpoint_.address().is_multicast() && target_endpoint_.protocol() == udp::v4() &&
                outbound_interface_) {
                socket_.set_option(
                    net::ip::multicast::outbound_interface(outbound_interface_->to_uint()), ec);
            }
            if (ec) {
                finish(ec, {});
                return;
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
        finish(ec, {});
        return;
    }

    std::string_view payload(buffer_.data(), bytes);
    if (payload.find(search_target_) != std::string_view::npos ||
        payload.find("HTTP/1.1 200") != std::string_view::npos) {
        finish({}, sender_);
        return;
    }
    schedule_receive();
}

inline void ssdp_resolver::handle_timeout(const boost::system::error_code& ec) {
    if (ec == net::error::operation_aborted) {
        return;
    }
    finish(make_error_code(net::error::timed_out), {});
}

inline void ssdp_resolver::finish(const boost::system::error_code& ec, udp::endpoint endpoint) {
    if (!resolving_) {
        return;
    }
    resolving_ = false;
    timer_.cancel();
    boost::system::error_code ignored;
    socket_.close(ignored);

    auto handler = std::move(handler_);
    handler_ = {};
    net::post(strand_, [handler = std::move(handler), ec, endpoint]() mutable {
        if (handler) {
            handler(ec, endpoint);
        }
    });
}

}  // namespace heos2mqtt
