#include "heos_client.hpp"
#include "mqtt_publisher.hpp"

#include <boost/asio.hpp>
#include <fmt/core.h>

#include <csignal>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>

namespace {

struct options {
    std::string heos_host{"127.0.0.1"};
    std::string heos_port{"1255"};
    std::string mqtt_host{"127.0.0.1"};
    std::string mqtt_port{"1883"};
    std::string base_topic{"heos"};
};

void print_usage(const char* name) {
    fmt::print(
        "Usage: {} [--heos-host HOST] [--heos-port PORT] [--mqtt-host HOST] "
        "[--mqtt-port PORT] [--base-topic TOPIC]\n",
        name);
}

options parse_args(int argc, char** argv) {
    options opts;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg{argv[i]};
        auto pop_value = [&](std::string& target) {
            if (i + 1 >= argc) {
                print_usage(argv[0]);
                std::exit(EXIT_FAILURE);
            }
            target = argv[++i];
        };

        if (arg == "--heos-host") {
            pop_value(opts.heos_host);
        } else if (arg == "--heos-port") {
            pop_value(opts.heos_port);
        } else if (arg == "--mqtt-host") {
            pop_value(opts.mqtt_host);
        } else if (arg == "--mqtt-port") {
            pop_value(opts.mqtt_port);
        } else if (arg == "--base-topic") {
            pop_value(opts.base_topic);
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            std::exit(EXIT_SUCCESS);
        } else {
            fmt::print(stderr, "Unknown argument: {}\n", arg);
            print_usage(argv[0]);
            std::exit(EXIT_FAILURE);
        }
    }
    return opts;
}

}  // namespace

int main(int argc, char** argv) {
    auto opts = parse_args(argc, argv);

    boost::asio::io_context io;
    auto work_guard = boost::asio::make_work_guard(io);

    heos2mqtt::mqtt_publisher publisher(io, opts.mqtt_host, opts.mqtt_port, opts.base_topic);
    heos2mqtt::heos_client client(
        io, opts.heos_host, opts.heos_port,
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
