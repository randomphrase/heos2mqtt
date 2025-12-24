#include "ssdp_resolver.hpp"

#include <boost/asio.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <optional>
#include <thread>

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

}  // namespace

TEST_CASE("ssdp_resolver receives multicast response", "[ssdp]") {
    boost::asio::io_context io;

    boost::asio::ip::udp::socket responder(io);
    responder.open(boost::asio::ip::udp::v4());
    responder.set_option(boost::asio::socket_base::reuse_address(true));
    responder.bind({boost::asio::ip::udp::v4(), 0});
    auto listen_port = responder.local_endpoint().port();
    responder.set_option(boost::asio::ip::multicast::enable_loopback(true));

    std::string response =
        "HTTP/1.1 200 OK\r\nST: urn:schemas-denon-com:device:ACT-Denon:1\r\n\r\n";

    auto buffer = std::array<char, 512>{};
    boost::asio::ip::udp::endpoint sender;

    responder.async_receive_from(
        boost::asio::buffer(buffer), sender,
        [&](const boost::system::error_code& ec, std::size_t /*bytes*/) {
            if (ec) {
                return;
            }
            responder.async_send_to(boost::asio::buffer(response), sender,
                                    [](const boost::system::error_code&, std::size_t) {});
        });

    heos2mqtt::ssdp_resolver resolver(io, {boost::asio::ip::address_v4::loopback(), listen_port});

    std::optional<boost::system::error_code> result_ec;
    std::optional<boost::asio::ip::udp::endpoint> result_endpoint;

    resolver.async_resolve("urn:schemas-denon-com:device:ACT-Denon:1", 1s,
                           [&](const boost::system::error_code& ec, boost::asio::ip::udp::endpoint ep) {
        result_ec = ec;
        result_endpoint = ep;
    });

    REQUIRE(run_until(io, 3s, [&]() { return result_ec.has_value(); }));
    REQUIRE(result_ec);
    REQUIRE_FALSE(result_ec->failed());
    REQUIRE(result_endpoint);
    REQUIRE(result_endpoint->port() > 0);
}
