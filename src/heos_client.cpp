#include "heos_client.hpp"
#include "logging/logging.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>

#include <fmt/core.h>
#include <fmt/chrono.h>

#include <algorithm>
#include <chrono>
#include <string_view>

namespace heos2mqtt {

using namespace std::chrono_literals;
using namespace logging;

heos_client::heos_client(
    std::string_view log_name,
    boost::asio::io_context& io,
    std::string host,
    std::string port,
    line_handler handler)
  : log_name_{log_name}
  , strand_(boost::asio::make_strand(io))
  , resolver_(io)
  , socket_(io)
  , reconnect_timer_(io)
  , host_(std::move(host))
  , port_(std::move(port))
  , handler_(std::move(handler))
{
    info("[{}] created for {}:{}", log_name_, host_, port_);
}

void heos_client::start() {
    boost::asio::dispatch(strand_, [this]() {
        if (started_) {
            return;
        }
        started_ = true;
        stopping_ = false;
        initiate_connect();
    });
}

void heos_client::stop() {
    boost::asio::dispatch(strand_, [this]() {
        stopping_ = true;
        reconnect_timer_.cancel();
        close_socket();
    });
}

void heos_client::set_reconnect_backoff(std::chrono::steady_clock::duration base,
                                        std::chrono::steady_clock::duration max) {
    if (base <= std::chrono::steady_clock::duration::zero()) {
        base = std::chrono::milliseconds(100);
    }
    if (max < base) {
        max = base;
    }
    boost::asio::dispatch(strand_, [this, base, max]() {
        reconnect_base_ = base;
        reconnect_max_ = max;
    });
}

void heos_client::initiate_connect() {
    if (stopping_) {
        return;
    }

    info("[{}]: resolving {}:{}", log_name_, host_, port_);
    resolver_.async_resolve(
        host_, port_,
        boost::asio::bind_executor(
            strand_, [this](const boost::system::error_code& ec,
                            boost::asio::ip::tcp::resolver::results_type results) {
                if (stopping_) {
                    return;
                }
                if (ec) {
                    error("[{}]: resolve error: {}", log_name_, ec.message());
                    schedule_reconnect();
                    return;
                }

                initiate_connect(std::move(results));
            }));
}

void heos_client::initiate_connect(boost::asio::ip::tcp::resolver::results_type results) {
    if (stopping_) {
        return;
    }

    info("[{}]: connecting...", log_name_);
    boost::asio::async_connect(
        socket_, std::move(results),
        boost::asio::bind_executor(
            strand_,
            [this](const boost::system::error_code& connect_ec,
                   const boost::asio::ip::tcp::endpoint&) {
                if (stopping_) {
                    return;
                }
                if (connect_ec) {
                    error("[{}]: connect error: {}", log_name_, connect_ec.message());
                    schedule_reconnect();
                    return;
                }

                info("[{}]: connected", log_name_);
                reconnect_attempts_ = 0;
                start_read();
            }));
}

void heos_client::start_read() {
    boost::asio::async_read_until(
        socket_, read_buffer_, '\n',
        boost::asio::bind_executor(
            strand_, [this](const boost::system::error_code& ec, std::size_t /*bytes_transferred*/) {
                if (stopping_) {
                    return;
                }

                if (ec) {
                    if (ec != boost::asio::error::operation_aborted) {
                        error("[{}]: read error: {}", log_name_, ec.message());
                    }
                    close_socket();
                    schedule_reconnect();
                    return;
                }

                std::istream stream(&read_buffer_);
                std::string line;
                std::getline(stream, line);
                // Trim carriage return if present.
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }

                if (handler_) {
                    handler_(line);
                }

                start_read();
            }));
}

void heos_client::schedule_reconnect() {
    if (stopping_) {
        return;
    }
    reconnect_attempts_ = std::min<std::size_t>(reconnect_attempts_ + 1, 32);
    auto exponent = std::min<std::size_t>(reconnect_attempts_, max_backoff_exponent_);
    auto multiplier = std::size_t{1} << exponent;
    auto delay = reconnect_base_ * multiplier;
    if (delay > reconnect_max_) {
        delay = reconnect_max_;
    }

    info("[{}]: reconnect in {}", log_name_, delay);
    reconnect_timer_.expires_after(delay);
    reconnect_timer_.async_wait(boost::asio::bind_executor(
        strand_, [this](const boost::system::error_code& ec) {
            if (!ec) {
                initiate_connect();
            }
        }));
}

void heos_client::close_socket() {
    boost::system::error_code ignored;
    socket_.close(ignored);
    read_buffer_.consume(read_buffer_.size());
}

}  // namespace heos2mqtt
