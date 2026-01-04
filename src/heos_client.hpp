#pragma once

#include <boost/asio.hpp>

#include <chrono>
#include <functional>
#include <string>

namespace heos2mqtt {

class heos_client {
public:
    using line_handler = std::function<void(std::string)>;

    heos_client(
        std::string_view log_name,
        boost::asio::io_context& io,
                std::string host,
                std::string port,
                line_handler handler);

    void start();
    void stop();
    void set_reconnect_backoff(std::chrono::steady_clock::duration base,
                               std::chrono::steady_clock::duration max);

private:
    void initiate_connect();
    void initiate_connect(boost::asio::ip::tcp::resolver::results_type results);
    void start_read();
    void schedule_reconnect();
    void close_socket();

    std::string log_name_;
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
    boost::asio::ip::tcp::resolver resolver_;
    boost::asio::ip::tcp::socket socket_;
    boost::asio::streambuf read_buffer_;
    boost::asio::steady_timer reconnect_timer_;
    std::string host_;
    std::string port_;
    line_handler handler_;
    bool started_{false};
    bool stopping_{false};
    std::size_t reconnect_attempts_{0};
    std::chrono::steady_clock::duration reconnect_base_{std::chrono::seconds(1)};
    std::chrono::steady_clock::duration reconnect_max_{std::chrono::seconds(30)};
    static constexpr std::size_t max_backoff_exponent_{5};
};

}  // namespace heos2mqtt
