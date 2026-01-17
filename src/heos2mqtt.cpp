#include "heos_client.hpp"
#include "mqtt_publisher.hpp"

#include <boost/asio.hpp>

#include <fmt/core.h>
#include <fmt/ostream.h>

#include <lyra/lyra.hpp>

#include <csignal>
#include <cstdlib>
#include <string>
#include <utility>

namespace {

struct options {
    std::string heos_host{"127.0.0.1"};
    std::string heos_port{"1255"};
    std::string mqtt_host{"127.0.0.1"};
    std::string mqtt_port{"1883"};
    std::string base_topic{"heos"};
};

options parse_args(int argc, const char** argv) {
    options opts;
    bool show_help = false;

    auto cli = lyra::cli()
        | lyra::help(show_help)
        | lyra::opt(opts.heos_host, "host").help("HEOS host address").optional()
            ["--heos-host"]
        | lyra::opt(opts.heos_port, "port").help("HEOS port").optional()
            ["--heos-port"]
        | lyra::opt(opts.mqtt_host, "host").help("MQTT host address").optional()
            ["--mqtt-host"]
        | lyra::opt(opts.mqtt_port, "port").help("MQTT port").optional()
            ["--mqtt-port"]
        | lyra::opt(opts.base_topic, "topic").help("MQTT base topic").optional()
            ["--base-topic"];

    auto result = cli.parse({argc, argv});
    if (!result) {
        fmt::print(stderr, "Error: {}\n", result.message());
        fmt::print(stderr, "{}\n", fmt::streamed(cli));
        std::exit(EXIT_FAILURE);
    }
    if (show_help) {
        fmt::print("{}\n", fmt::streamed(cli));
        std::exit(EXIT_SUCCESS);
    }
    return opts;
}

}  // namespace

int main(int argc, const char** argv) {
    auto opts = parse_args(argc, argv);

    boost::asio::io_context io;
    auto work_guard = boost::asio::make_work_guard(io);

    heos2mqtt::mqtt_publisher publisher(io, opts.mqtt_host, opts.mqtt_port, opts.base_topic);
    auto heos_port = static_cast<boost::asio::ip::port_type>(std::stoul(opts.heos_port));
    heos2mqtt::heos_client client("HEOS",
        io, opts.heos_host, heos_port,
        [&publisher](std::string line) { publisher.publish_raw(std::move(line)); });

    boost::asio::signal_set signals(io, SIGINT, SIGTERM);
    signals.async_wait([&](const boost::system::error_code& ec, int signal_number) {
        if (!ec) {
            fmt::print("Received signal {}. Shutting down...\n", signal_number);
            client.stop();
            publisher.stop();
            work_guard.reset();
        }
    });

    fmt::print("Starting heos2mqtt. HEOS {}:{} -> MQTT {}:{} (topic: {})\n", opts.heos_host,
               opts.heos_port, opts.mqtt_host, opts.mqtt_port, opts.base_topic);

    publisher.start();
    client.start();

    io.run();
    fmt::print("Clean shutdown complete.\n");
    return 0;
}
