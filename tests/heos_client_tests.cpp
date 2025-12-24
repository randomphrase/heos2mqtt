#include "heos_client.hpp"

#include <boost/asio.hpp>
#include <catch2/catch_test_macros.hpp>
#include <fmt/core.h>

#include <chrono>
#include <deque>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace {

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
            std::this_thread::sleep_for(1ms);
        }
    }
    return predicate();
}

void run_for(boost::asio::io_context& io, clock_type::duration duration) {
    auto end = clock_type::now() + duration;
    run_until(io, duration, [&]() { return clock_type::now() >= end; });
}

class mock_heos_server {
public:
    mock_heos_server(boost::asio::io_context& io, std::uint16_t port)
        : io_(io),
          acceptor_(io, {boost::asio::ip::tcp::v4(), port}) {}

    struct batch {
        std::vector<std::string> lines;
        bool close_after{true};
        std::size_t index{0};
    };

    void enqueue(batch batch_item) {
        batches_.push_back(std::move(batch_item));
    }

    std::uint16_t port() const {
        return acceptor_.local_endpoint().port();
    }

    void start() {
        accept_next();
    }

    void stop() {
        boost::system::error_code ec;
        acceptor_.close(ec);
        if (socket_) {
            socket_->close(ec);
        }
    }

private:
    void accept_next() {
        acceptor_.async_accept([this](const boost::system::error_code& ec, boost::asio::ip::tcp::socket socket) {
            if (ec) {
                return;
            }
            socket_ = std::make_unique<boost::asio::ip::tcp::socket>(std::move(socket));
            if (batches_.empty()) {
                return;
            }

            auto batch_item = std::make_shared<batch>(std::move(batches_.front()));
            batches_.pop_front();
            send_batch(batch_item);
        });
    }

    void send_batch(const std::shared_ptr<batch>& batch_item) {
        if (!socket_) {
            return;
        }
        if (batch_item->index >= batch_item->lines.size()) {
            if (batch_item->close_after) {
                boost::system::error_code ec;
                socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
                socket_->close(ec);
                socket_.reset();
                accept_next();
            }
            return;
        }

        auto line = batch_item->lines[batch_item->index++] + "\r\n";
        boost::asio::async_write(
            *socket_, boost::asio::buffer(line),
            [this, batch_item](const boost::system::error_code& ec, std::size_t /*bytes*/) {
                if (ec) {
                    socket_.reset();
                    accept_next();
                    return;
                }
                send_batch(batch_item);
            });
    }

    boost::asio::io_context& io_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::unique_ptr<boost::asio::ip::tcp::socket> socket_;
    std::deque<batch> batches_;
};

}  // namespace

TEST_CASE("heos_client streams lines in order", "[heos-client]") {
    boost::asio::io_context io;

    mock_heos_server server(io, 0);
    server.enqueue({{"line1", "line2", "line3"}, false});
    server.start();

    std::vector<std::string> received;

    heos2mqtt::heos_client client(
        io, "127.0.0.1", std::to_string(server.port()),
        [&](std::string line) { received.push_back(std::move(line)); });

    client.set_reconnect_backoff(50ms, 200ms);
    client.start();

    REQUIRE(run_until(io, 500ms, [&]() {
        return received.size() == 3;
    }));

    REQUIRE(received == std::vector<std::string>{"line1", "line2", "line3"});

    client.stop();
    server.stop();
    run_for(io, 200ms);
}

TEST_CASE("heos_client reconnects after disconnect", "[heos-client]") {
    boost::asio::io_context io;

    mock_heos_server server(io, 0);
    server.enqueue({{"first"}, true});
    server.enqueue({{"second"}, false});
    server.start();

    std::vector<std::string> received;

    heos2mqtt::heos_client client(
        io, "127.0.0.1", std::to_string(server.port()),
        [&](std::string line) { received.push_back(std::move(line)); });

    client.set_reconnect_backoff(50ms, 200ms);
    client.start();

    REQUIRE(run_until(io, 4s, [&]() {
        return received.size() == 2;
    }));

    REQUIRE(received == std::vector<std::string>{"first", "second"});

    client.stop();
    server.stop();
    run_for(io, 500ms);
}

TEST_CASE("heos_client stop is idempotent", "[heos-client]") {
    boost::asio::io_context io;

    mock_heos_server server(io, 0);
    server.start();

    heos2mqtt::heos_client client(io, "127.0.0.1", std::to_string(server.port()),
                                  [](std::string) {});

    client.set_reconnect_backoff(50ms, 200ms);
    client.start();

    run_for(io, 200ms);

    client.stop();
    client.stop();

    server.stop();
    run_for(io, 200ms);

    SUCCEED("Stop completed without deadlock");
}
