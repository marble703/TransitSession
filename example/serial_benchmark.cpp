#include "example/latency_stats.hpp"
#include "transport/serial/serial_session.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct Options {
    std::string device_path = "/dev/ttyACM0";
    unsigned int baud_rate = 115200;
    std::size_t payload_size = 32;
    std::size_t samples = 1000;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(1000);
};

bool parse_unsigned(const char* text, std::size_t& value) {
    try {
        value = std::stoull(text);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool parse_unsigned(const char* text, unsigned int& value) {
    std::size_t parsed = 0;
    if (!parse_unsigned(text, parsed)) {
        return false;
    }
    value = static_cast<unsigned int>(parsed);
    return true;
}

bool parse_options(int argc, char* argv[], Options& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--device" && i + 1 < argc) {
            options.device_path = argv[++i];
        } else if (arg == "--baud" && i + 1 < argc) {
            if (!parse_unsigned(argv[++i], options.baud_rate)) {
                return false;
            }
        } else if (arg == "--payload" && i + 1 < argc) {
            if (!parse_unsigned(argv[++i], options.payload_size)) {
                return false;
            }
        } else if (arg == "--samples" && i + 1 < argc) {
            if (!parse_unsigned(argv[++i], options.samples)) {
                return false;
            }
        } else if (arg == "--timeout-ms" && i + 1 < argc) {
            std::size_t timeout_ms = 0;
            if (!parse_unsigned(argv[++i], timeout_ms)) {
                return false;
            }
            options.timeout = std::chrono::milliseconds(timeout_ms);
        } else if (arg == "--help") {
            return false;
        } else {
            return false;
        }
    }

    return options.payload_size >= sizeof(std::uint32_t) && options.samples > 0;
}

void print_usage() {
    std::cerr
        << "Usage: TransitSessionSerialBenchmark --device <path> [--baud <rate>] "
        << "[--payload <bytes>] [--samples <count>] [--timeout-ms <ms>]\n"
        << "The target device should echo each request back unchanged.\n";
}

std::vector<char> make_payload(std::size_t payload_size, std::uint32_t sequence) {
    std::vector<char> payload(payload_size, 0);
    std::memcpy(payload.data(), &sequence, sizeof(sequence));
    for (std::size_t i = sizeof(sequence); i < payload.size(); ++i) {
        payload[i] = static_cast<char>(i & 0xffU);
    }
    return payload;
}

} // namespace

int main(int argc, char* argv[]) {
    Options options;
    if (!parse_options(argc, argv, options)) {
        print_usage();
        return 1;
    }

    boost::asio::io_context io_context;
    session::serial::SerialPortConfig config;
    config.device_path = options.device_path;
    config.baud_rate = options.baud_rate;
    config.duplex_mode = session::serial::DuplexMode::full_duplex;
    config.read_buffer_size = options.payload_size;

    boost::system::error_code ec;
    auto serial_session = session::serial::SerialSession::create(io_context, config, &ec);
    if (ec) {
        std::cerr << "Error opening serial port: " << ec.message() << "\n";
        return 1;
    }

    benchmark::LatencyStats stats;
    std::vector<char> pending_response;
    pending_response.reserve(options.payload_size * 2U);

    std::atomic<bool> failed = false;
    std::atomic<bool> completed = false;
    std::size_t sent_count = 0;
    std::size_t received_count = 0;
    std::uint32_t expected_sequence = 0;
    Clock::time_point send_started_at {};

    auto send_next = [&]() {
        if (sent_count >= options.samples) {
            return;
        }

        expected_sequence = static_cast<std::uint32_t>(sent_count);
        auto payload = make_payload(options.payload_size, expected_sequence);
        send_started_at = Clock::now();
        serial_session->send(std::move(payload));
        ++sent_count;
    };

    serial_session->set_read_handler(
        [&](boost::system::error_code read_ec, std::vector<char> data) {
            if (read_ec) {
                std::cerr << "Serial read error: " << read_ec.message() << "\n";
                failed = true;
                completed = true;
                serial_session->close();
                io_context.stop();
                return;
            }

            pending_response.insert(pending_response.end(), data.begin(), data.end());

            while (pending_response.size() >= options.payload_size) {
                std::uint32_t actual_sequence = 0;
                std::memcpy(&actual_sequence, pending_response.data(), sizeof(actual_sequence));
                pending_response.erase(
                    pending_response.begin(),
                    pending_response.begin()
                        + static_cast<std::vector<char>::difference_type>(options.payload_size)
                );

                if (actual_sequence != expected_sequence) {
                    std::cerr << "Serial benchmark out-of-order echo: expected sequence "
                              << expected_sequence << ", got " << actual_sequence << "\n";
                    failed = true;
                    completed = true;
                    serial_session->close();
                    io_context.stop();
                    return;
                }

                stats.add(std::chrono::duration_cast<benchmark::LatencyStats::Duration>(
                    Clock::now() - send_started_at
                ));
                ++received_count;

                if (received_count >= options.samples) {
                    completed = true;
                    serial_session->close();
                    io_context.stop();
                    return;
                }

                send_next();
            }
        }
    );

    serial_session->start_reading();
    send_next();

    std::thread timeout_thread([&]() {
        const auto start = Clock::now();
        while (!completed.load()) {
            if (Clock::now() - start >= options.timeout * static_cast<int>(options.samples)) {
                std::cerr << "Serial benchmark timeout after " << received_count
                          << " completed samples\n";
                failed = true;
                completed = true;
                serial_session->close();
                io_context.stop();
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    io_context.run();
    completed = true;
    timeout_thread.join();

    if (failed.load()) {
        return 1;
    }

    stats.print_summary("serial_rtt");
    return 0;
}
