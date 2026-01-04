#include "expect_calls.hpp"
#include "run_until.hpp"
#include "ssdp_responder.hpp"
#include "ssdp_resolver.hpp"

#include <boost/asio.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <chrono>

using namespace std::chrono_literals;
using Catch::Matchers::ContainsSubstring;

TEST_CASE("ssdp_resolver receives multicast response", "[ssdp]") {
    boost::asio::io_context io;

    test::ssdp_responder responder(io);

    std::string response =
        "HTTP/1.1 200 OK\r\nST: urn:schemas-denon-com:device:ACT-Denon:1\r\n\r\n";
    bool resolved = false;

    heos2mqtt::ssdp_resolver resolver(io, responder.endpoint());

    resolver.async_resolve(
        "urn:schemas-denon-com:device:ACT-Denon:1", 1s,
        test::expect_calls(
            1, [&](const boost::system::error_code& ec, const boost::asio::ip::udp::endpoint& ep) {
                resolved = true;
                CHECK_FALSE(ec.failed());
                CHECK(ep.port() > 0);
            }));

    auto req = responder.expect_request();
    CHECK_THAT(req.payload_, ContainsSubstring("M-SEARCH"));

    responder.send_response(response, req.sender_);
    test::run_until(io, [&]() { return resolved; });

    test::run_remaining(io);
}
