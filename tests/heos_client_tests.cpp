#include "heos_client.hpp"

#include "run_until.hpp"
#include "ssdp_responder.hpp"

#include <boost/asio.hpp>
#include <catch2/catch_test_macros.hpp>
#include <fmt/core.h>

#include <chrono>
#include <deque>
#include <string>
#include <vector>

using namespace std::chrono_literals;

namespace {

constexpr std::string_view heos_ssdp_response =
    "HTTP/1.1 200 OK\r\nST: urn:schemas-denon-com:device:ACT-Denon:1\r\n\r\n";
constexpr std::string_view other_ssdp_response =
    "HTTP/1.1 200 OK\r\nST: urn:schemas-denon-com:device:OTHER\r\n\r\n";

class mock_heos_server {
public:
    mock_heos_server(boost::asio::io_context& io, std::uint16_t port)
    : acceptor_(io, {boost::asio::ip::tcp::v4(), port})
    , socket_(io)
    {}

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
        socket_.close(ec);
    }

private:
    void accept_next() {
        acceptor_.async_accept([this](const boost::system::error_code& ec, boost::asio::ip::tcp::socket socket) {
            if (ec) {
                return;
            }
            socket_ = std::move(socket);
            if (batches_.empty()) {
                return;
            }

            auto batch_item = std::make_shared<batch>(std::move(batches_.front()));
            batches_.pop_front();
            send_batch(batch_item);
        });
    }

    void send_batch(const std::shared_ptr<batch>& batch_item) {
        if (batch_item->index_ >= batch_item->lines_.size()) {
            if (batch_item->close_after_) {
                boost::system::error_code ec;
                socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
                socket_.close(ec);
                accept_next();
            }
            return;
        }

        auto line = batch_item->lines_[batch_item->index_++] + "\r\n";
        boost::asio::async_write(
            socket_, boost::asio::buffer(line),
            [this, batch_item](const boost::system::error_code& ec, std::size_t /*bytes*/) {
                if (ec) {
                    accept_next();
                    return;
                }
                send_batch(batch_item);
            });
    }

    boost::asio::ip::tcp::acceptor acceptor_;
    boost::asio::ip::tcp::socket socket_;
    std::deque<batch> batches_;
};

}  // namespace

TEST_CASE("heos_client streams lines in order", "[heos-client]") {
    boost::asio::io_context io;

    mock_heos_server server(io, 0);
    server.enqueue({{"line1", "line2", "line3"}, false});
    server.start();

    std::vector<std::string> received;
    constexpr std::string_view device_name = "living_room";
    test::ssdp_responder responder(io);

    heos2mqtt::heos_client client("test_client",
        io, std::string(device_name), std::to_string(server.port()),
        [&](std::string line) { received.push_back(std::move(line)); },
        responder.endpoint());

    client.set_reconnect_backoff(50ms, 200ms);
    client.start();

    auto req = responder.expect_request();
    responder.send_response(
        heos_ssdp_response,
        req.sender_);

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
    constexpr std::string_view device_name = "living_room";
    test::ssdp_responder responder(io);

    heos2mqtt::heos_client client("test_client",
        io, std::string(device_name), std::to_string(server.port()),
        [&](std::string line) { received.push_back(std::move(line)); },
        responder.endpoint());

    client.set_reconnect_backoff(50ms, 200ms);
    client.start();

    auto req = responder.expect_request();
    responder.send_response(
        heos_ssdp_response,
        req.sender_);
    auto req2 = responder.expect_request();
    responder.send_response(
        heos_ssdp_response,
        req2.sender_);

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

    constexpr std::string_view device_name = "living_room";
    test::ssdp_responder responder(io);

    heos2mqtt::heos_client client("test_client", io, std::string(device_name), std::to_string(server.port()),
                                  [](std::string) {}, responder.endpoint());

    client.set_reconnect_backoff(50ms, 200ms);
    client.start();

    auto req = responder.expect_request();
    responder.send_response(
        heos_ssdp_response,
        req.sender_);

    test::run_for(io, 200ms);

    client.stop();
    client.stop();

    server.stop();
    test::run_for(io, 200ms);

    SUCCEED("Stop completed without deadlock");

    test::run_remaining(io);
}

TEST_CASE("heos_client retries after non-matching SSDP response", "[heos-client]") {
    boost::asio::io_context io;

    mock_heos_server server(io, 0);
    server.enqueue({{"line1"}, false});
    server.start();

    std::vector<std::string> received;
    constexpr std::string_view device_name = "living_room";
    test::ssdp_responder responder(io);

    heos2mqtt::heos_client client("test_client",
        io, std::string(device_name), std::to_string(server.port()),
        [&](std::string line) { received.push_back(std::move(line)); },
        responder.endpoint());

    client.set_reconnect_backoff(10ms, 50ms);
    client.start();

    auto req = responder.expect_request();
    responder.send_response(
        other_ssdp_response,
        req.sender_);
    auto req2 = responder.expect_request();
    responder.send_response(
        heos_ssdp_response,
        req2.sender_);

    test::run_until(io, [&]() {
        return received.size() == 1;
    });

    REQUIRE(received == std::vector<std::string>{"line1"});

    client.stop();
    server.stop();
    test::run_remaining(io);
}
