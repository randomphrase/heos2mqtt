#include "logging/logging.hpp"
#include "ssdp/ssdp_resolver.hpp"

#include <boost/asio/io_context.hpp>

#include <fmt/core.h>
#include <fmt/ostream.h>

#include <lyra/lyra.hpp>

#include <chrono>
#include <string>

using namespace std::chrono_literals;

int main(int argc, const char** argv)
try {
    std::string query;
    bool show_help = false;

    auto cli = lyra::cli()
        | lyra::help(show_help)
        | lyra::opt([] (bool) {
            logging::logger::get_default().set_min_level(logging::severity::debug);
          }).help("Enable verbose logging")
          ["-v"]["--verbose"]
        | lyra::arg(query, "st").required().help("SSDP search target")
    ;

    auto result = cli.parse({argc, argv});
    if (!result) {
        fmt::print(stderr, "Error: {}\n", result.message());
        fmt::print(stderr, "{}\n", fmt::streamed(cli));
        return 1;
    }
    if (show_help) {
        fmt::print("{}\n", fmt::streamed(cli));
        return 0;
    }

    boost::asio::io_context io;
    ssdp::ssdp_resolver resolver(io);
    int exit_code = 1;

    resolver.async_resolve(
        query, 3s,
        [&](const boost::system::error_code& ec, const boost::asio::ip::address& address) {
            if (ec) {
                fmt::print(stderr, "SSDP resolve failed: {}\n", ec.message());
                exit_code = 1;
            } else {
                fmt::print("{}\n", fmt::streamed(address));
                exit_code = 0;
            }
            io.stop();
        });

    io.run();
    return exit_code;

} catch (const std::exception& ex) {
    fmt::print(stderr, "Unhandled exception: {}\n", ex.what());
    return 1;
}
