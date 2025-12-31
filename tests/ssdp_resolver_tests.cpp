#include "expect_calls.hpp"
#include "run_until.hpp"
#include "ssdp_responder.hpp"
#include "ssdp_resolver.hpp"

#include <boost/asio.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>

using namespace std::chrono_literals;

TEST_CASE("ssdp_resolver receives multicast response", "[ssdp]") {
    boost::asio::io_context io;

    test::ssdp_responder responder(io, {boost::asio::ip::udp::v4(), 0});

    std::string response =
        "HTTP/1.1 200 OK\r\nST: urn:schemas-denon-com:device:ACT-Denon:1\r\n\r\n";
    bool resolved = false;
    boost::system::error_code resolved_ec;
    boost::asio::ip::udp::endpoint resolved_ep;

    heos2mqtt::ssdp_resolver resolver(
        io, {boost::asio::ip::address_v4::loopback(), responder.port()});

    resolver.async_resolve(
        "urn:schemas-denon-com:device:ACT-Denon:1", 1s,
        test::expect_calls(
            1, [&](const boost::system::error_code& ec, boost::asio::ip::udp::endpoint ep) {
                resolved = true;
                resolved_ec = ec;
                resolved_ep = ep;
                CHECK_FALSE(ec.failed());
                CHECK(ep.port() > 0);
            }));

    REQUIRE(responder.expect_request(500ms, "M-SEARCH", response));
    REQUIRE(test::run_until(io, 1s, [&]() { return resolved; }));
    CHECK_FALSE(resolved_ec.failed());
    CHECK(resolved_ep.port() > 0);
}
