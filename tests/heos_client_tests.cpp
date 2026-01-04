#include "heos_client.hpp"

#include "run_until.hpp"

#include <boost/asio.hpp>
#include <catch2/catch_test_macros.hpp>
#include <fmt/core.h>

#include <chrono>
#include <deque>
#include <string>
#include <vector>

using namespace std::chrono_literals;

namespace {

class mock_heos_server {
public:
    mock_heos_server(boost::asio::io_context& io, std::uint16_t port)
        : acceptor_(io, {boost::asio::ip::tcp::v4(), port}) {}

    struct batch {
        std::vector<std::string> lines_;
        bool close_after_{true};
        std::size_t index_{0};
    };

    void enqueue(batch batch_item) {
        batches_.push_back(std::move(batch_item));
    }

    [[nodiscard]] std::uint16_t port() const {
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
        if (batch_item->index_ >= batch_item->lines_.size()) {
            if (batch_item->close_after_) {
                boost::system::error_code ec;
                socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
                socket_->close(ec);
                socket_.reset();
                accept_next();
            }
            return;
        }

        auto line = batch_item->lines_[batch_item->index_++] + "\r\n";
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

    heos2mqtt::heos_client client("test_client",
        io, "127.0.0.1", std::to_string(server.port()),
        [&](std::string line) { received.push_back(std::move(line)); });

    client.set_reconnect_backoff(50ms, 200ms);
    client.start();

    test::run_until(io, [&]() {
        return received.size() == 3;
    });

    REQUIRE(received == std::vector<std::string>{"line1", "line2", "line3"});

    client.stop();
    server.stop();
    test::run_for(io, 200ms);
}

TEST_CASE("heos_client reconnects after disconnect", "[heos-client]") {
    boost::asio::io_context io;

    mock_heos_server server(io, 0);
    server.enqueue({{"first"}, true});
    server.enqueue({{"second"}, false});
    server.start();

    std::vector<std::string> received;

    heos2mqtt::heos_client client("test_client",
        io, "127.0.0.1", std::to_string(server.port()),
        [&](std::string line) { received.push_back(std::move(line)); });

    client.set_reconnect_backoff(50ms, 200ms);
    client.start();

    test::run_until(io, [&]() {
        return received.size() == 2;
    });

    REQUIRE(received == std::vector<std::string>{"first", "second"});

    client.stop();
    server.stop();
    test::run_remaining(io);
}

TEST_CASE("heos_client stop is idempotent", "[heos-client]") {
    boost::asio::io_context io;

    mock_heos_server server(io, 0);
    server.start();

    heos2mqtt::heos_client client("test_client", io, "127.0.0.1", std::to_string(server.port()),
                                  [](std::string) {});

    client.set_reconnect_backoff(50ms, 200ms);
    client.start();

    test::run_for(io, 200ms);

    client.stop();
    client.stop();

    server.stop();
    test::run_for(io, 200ms);

    SUCCEED("Stop completed without deadlock");

    test::run_remaining(io);
}
