#pragma once

#include <boost/asio.hpp>
#include <boost/json.hpp>
#include <boost/mqtt5/mqtt_client.hpp>

#include <optional>
#include <string>

namespace heos2mqtt {

namespace mqtt = boost::mqtt5;

class mqtt_publisher;

namespace detail {

class mqtt_logger {
public:
    explicit mqtt_logger(mqtt_publisher& owner) : owner_(&owner) {}

    void at_connack(mqtt::reason_code rc,
                    bool session_present,
                    const mqtt::connack_props& props);
    void at_disconnect(mqtt::reason_code rc, const mqtt::disconnect_props& props);
    void at_transport_error(mqtt::error_code ec);

private:
    mqtt_publisher* owner_{nullptr};
};

}  // namespace detail

class mqtt_publisher {
    friend class detail::mqtt_logger;

public:
    mqtt_publisher(boost::asio::io_context& io,
                   std::string host,
                   std::string port,
                   std::string base_topic);

    void start();
    void stop();
    void publish_raw(std::string line);

private:
    using client_type =
        mqtt::mqtt_client<boost::asio::ip::tcp::socket, std::monostate, detail::mqtt_logger>;

    void ensure_client();
    void run_client();
    void handle_run_complete(mqtt::error_code ec);
    void schedule_restart();
    void handle_connack(mqtt::reason_code rc, bool session_present, const mqtt::connack_props& props);
    void handle_disconnect_notice(mqtt::reason_code rc, const mqtt::disconnect_props& props);
    void handle_transport_error(mqtt::error_code ec);
    std::uint16_t default_port() const;
    std::string build_topic(const std::string& suffix) const;

    boost::asio::io_context& io_;
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
    std::string host_;
    std::string port_;
    std::string base_topic_;
    std::string client_id_;
    boost::asio::steady_timer reconnect_timer_;
    std::optional<client_type> client_;
    bool running_{false};
    bool connected_{false};
    bool stopping_{false};
    std::size_t reconnect_attempts_{0};
};

}  // namespace heos2mqtt
