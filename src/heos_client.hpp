#pragma once

#include "ssdp_resolver.hpp"

#include <boost/asio.hpp>

#include <chrono>
#include <functional>
#include <string>
#include <string_view>

namespace heos2mqtt {

class heos_client {
public:
    using tcp = boost::asio::ip::tcp;

    using line_handler = std::function<void(std::string)>;

    heos_client(
        std::string_view log_name,
        boost::asio::io_context& io,
        std::string device_label,
        std::string port,
        line_handler handler,
        boost::asio::ip::udp::endpoint ssdp_endpoint = default_ssdp_endpoint);

    void start();
    void stop();
    void set_reconnect_backoff(std::chrono::steady_clock::duration base,
                               std::chrono::steady_clock::duration max);

private:
    void initiate_resolve();
    void initiate_connect();
    void initiate_connect(tcp::resolver::results_type&& results);
    void start_read();
    void schedule_reconnect();
    void close_socket();

    std::string log_name_;
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
    ssdp_resolver ssdp_resolver_;
    tcp::resolver resolver_;
    tcp::socket socket_;
    boost::asio::streambuf read_buffer_;
    boost::asio::steady_timer reconnect_timer_;
    std::string device_label_;
    std::string host_;
    std::string port_;
    line_handler handler_;
    bool started_{false};
    bool stopping_{false};
    std::size_t reconnect_attempts_{0};
    std::chrono::steady_clock::duration reconnect_base_{std::chrono::seconds(1)};
    std::chrono::steady_clock::duration reconnect_max_{std::chrono::seconds(30)};

    static constexpr std::size_t max_backoff_exponent{5};
};

}  // namespace heos2mqtt
