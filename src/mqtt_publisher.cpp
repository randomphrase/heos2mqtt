#include "mqtt_publisher.hpp"

#include <fmt/core.h>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <limits>
#include <random>
#include <sstream>

namespace heos2mqtt {

using namespace std::chrono_literals;

namespace {

std::string random_id() {
    std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(0, 15);
    std::ostringstream oss;
    for (int i = 0; i < 6; ++i) {
        oss << std::hex << dist(rng);
    }
    return oss.str();
}

std::string current_iso_timestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&t, &tm);
    std::array<char, 64> buffer; // NOLINT(cppcoreguidelines-pro-type-member-init)
    std::strftime(buffer.begin(), buffer.size(), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buffer.data();
}

}  // namespace

void detail::mqtt_logger::at_connack(mqtt::reason_code rc,
                                     bool session_present,
                                     const mqtt::connack_props& props) {
    if (owner_) {
        owner_->handle_connack(rc, session_present, props);
    }
}

void detail::mqtt_logger::at_disconnect(mqtt::reason_code rc,
                                        const mqtt::disconnect_props& props) {
    if (owner_) {
        owner_->handle_disconnect_notice(rc, props);
    }
}

void detail::mqtt_logger::at_transport_error(mqtt::error_code ec) {
    if (owner_) {
        owner_->handle_transport_error(ec);
    }
}

mqtt_publisher::mqtt_publisher(boost::asio::io_context& io,
                               std::string host,
                               std::string port,
                               std::string base_topic)
    : strand_(boost::asio::make_strand(io)),
      host_(std::move(host)),
      port_(std::move(port)),
      base_topic_(std::move(base_topic)),
      client_id_(fmt::format("heos2mqtt-{}", random_id())),
      reconnect_timer_(io),
      client_(io, std::monostate{}, detail::mqtt_logger(*this))
{}

void mqtt_publisher::start() {
    boost::asio::dispatch(strand_, [this]() {
        if (running_) {
            return;
        }
        stopping_ = false;
        running_ = true;
        reconnect_attempts_ = 0;
        reconnect_timer_.cancel();
        ensure_client();
        run_client();
    });
}

void mqtt_publisher::stop() {
    boost::asio::dispatch(strand_, [this]() {
        stopping_ = true;
        running_ = false;
        reconnect_timer_.cancel();
        connected_ = false;
        client_.async_disconnect(
            mqtt::disconnect_rc_e::normal_disconnection,
            mqtt::disconnect_props{},
            boost::asio::bind_executor(
                strand_, [](mqtt::error_code ec) {
                if (ec && ec != boost::asio::error::operation_aborted) {
                    fmt::print(stderr, "MQTT: disconnect error: {}\n", ec.message());
                }
            }));
    });
}

void mqtt_publisher::publish_raw(std::string line) {
    boost::asio::dispatch(strand_, [this, line = std::move(line)]() {
        if (!connected_) {
            return;
        }
        boost::json::object payload{
            {"raw", line},
            {"ts", current_iso_timestamp()},
        };
        auto serialized = boost::json::serialize(payload);
        mqtt::publish_props props;
        client_.async_publish<mqtt::qos_e::at_least_once>(
            build_topic("raw"), std::move(serialized), mqtt::retain_e::no, props,
            boost::asio::bind_executor(
                strand_,
                [](mqtt::error_code ec, mqtt::reason_code rc, mqtt::puback_props) {
                    if (ec) {
                        fmt::print(stderr, "MQTT: publish error: {} ({})\n", ec.message(), rc.message());
                    }
                }));
    });
}

void mqtt_publisher::ensure_client() {
    client_.brokers(fmt::format("{}:{}", host_, port_), default_port());
    client_.credentials(client_id_);
    client_.keep_alive(30);
}

void mqtt_publisher::run_client() {
    fmt::print("MQTT: starting client run to {}:{}\n", host_, port_);
    client_.async_run(boost::asio::bind_executor(
        strand_, [this](mqtt::error_code ec) { handle_run_complete(ec); }));
}

void mqtt_publisher::handle_run_complete(mqtt::error_code ec) {
    connected_ = false;
    if (stopping_) {
        if (ec && ec != boost::asio::error::operation_aborted) {
            fmt::print(stderr, "MQTT: run stopped ({})\n", ec.message());
        }
        reconnect_attempts_ = 0;
        return;
    }
    fmt::print(stderr, "MQTT: client run ended ({})\n", ec.message());
    schedule_restart();
}

void mqtt_publisher::schedule_restart() {
    if (stopping_ || !running_) {
        return;
    }
    reconnect_attempts_ = std::min<std::size_t>(reconnect_attempts_ + 1, 6);
    auto delay = std::chrono::seconds(3 * reconnect_attempts_);
    fmt::print("MQTT: restarting in {}s\n", delay.count());
    reconnect_timer_.expires_after(delay);
    reconnect_timer_.async_wait(boost::asio::bind_executor(
        strand_, [this](const boost::system::error_code& ec) {
            if (!ec && running_) {
                ensure_client();
                run_client();
            }
        }));
}

void mqtt_publisher::handle_connack(mqtt::reason_code rc,
                                    bool /*session_present*/,
                                    const mqtt::connack_props& /*props*/) {
    boost::asio::dispatch(strand_, [this, rc]() {
        if (!running_) {
            return;
        }
        if (rc == mqtt::reason_codes::success) {
            fmt::print("MQTT: connected\n");
            connected_ = true;
            reconnect_attempts_ = 0;
        } else {
            fmt::print(stderr, "MQTT: connack error: {}\n", rc.message());
        }
    });
}

void mqtt_publisher::handle_disconnect_notice(mqtt::reason_code rc,
                                              const mqtt::disconnect_props& /*props*/) {
    boost::asio::dispatch(strand_, [this, rc]() {
        if (stopping_) {
            return;
        }
        connected_ = false;
        fmt::print(stderr, "MQTT: disconnected ({})\n", rc.message());
    });
}

void mqtt_publisher::handle_transport_error(mqtt::error_code ec) {
    boost::asio::dispatch(strand_, [this, ec]() {
        if (stopping_) {
            return;
        }
        connected_ = false;
        fmt::print(stderr, "MQTT: transport error: {}\n", ec.message());
    });
}

std::uint16_t mqtt_publisher::default_port() const {
    auto value = std::stoi(port_);
    if (value >= 0 && value <= std::numeric_limits<std::uint16_t>::max()) {
        return static_cast<std::uint16_t>(value);
    }
    return 1883;
}

std::string mqtt_publisher::build_topic(const std::string& suffix) const {
    if (base_topic_.empty()) {
        return suffix;
    }
    return fmt::format("{}/{}", base_topic_, suffix);
}

}  // namespace heos2mqtt
