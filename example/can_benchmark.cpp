#include "example/latency_stats.hpp"
#include "transport/can/can_session.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

namespace {

using Clock = std::chrono::steady_clock;

struct Options {
    std::string interface_name = "can0";
    std::uint32_t can_id = 0x123U;
    std::size_t samples = 1000;
    bool fd_frame = false;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(1000);
};

bool parse_unsigned(const char* text, std::size_t& value) {
    try {
        value = std::stoull(text, nullptr, 0);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool parse_unsigned(const char* text, std::uint32_t& value) {
    std::size_t parsed = 0;
    if (!parse_unsigned(text, parsed)) {
        return false;
    }
    value = static_cast<std::uint32_t>(parsed);
    return true;
}

bool parse_options(int argc, char* argv[], Options& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--if" && i + 1 < argc) {
            options.interface_name = argv[++i];
        } else if (arg == "--id" && i + 1 < argc) {
            if (!parse_unsigned(argv[++i], options.can_id)) {
                return false;
            }
        } else if (arg == "--samples" && i + 1 < argc) {
            if (!parse_unsigned(argv[++i], options.samples)) {
                return false;
            }
        } else if (arg == "--fd") {
            options.fd_frame = true;
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

    return options.samples > 0;
}

void print_usage() {
    std::cerr << "Usage: TransitSessionCanBenchmark [--if <canX>] [--id <hex-or-dec>] "
                 "[--samples <count>] [--fd] [--timeout-ms <ms>]\n"
              << "This benchmark uses SocketCAN local loopback and requires loopback enabled.\n";
}

session::can::CanFrame make_frame(std::uint32_t can_id, std::uint32_t sequence, bool fd_frame) {
    session::can::CanFrame frame;
    frame.id = can_id;
    frame.fd_frame = fd_frame;
    frame.data_length = fd_frame ? 16U : 8U;
    frame.bitrate_switch = fd_frame;
    std::memcpy(frame.data.data(), &sequence, sizeof(sequence));
    for (std::size_t i = sizeof(sequence); i < frame.data_length; ++i) {
        frame.data[i] = static_cast<std::uint8_t>(i & 0xffU);
    }
    return frame;
}

} // namespace

int main(int argc, char* argv[]) {
    Options options;
    if (!parse_options(argc, argv, options)) {
        print_usage();
        return 1;
    }

    boost::asio::io_context io_context;
    session::can::CanConfig config;
    config.interface_name = options.interface_name;
    config.enable_can_fd = options.fd_frame;
    config.loopback = true;
    config.receive_own_messages = true;
    config.filters.push_back(session::can::CanFilter {
        .id = options.can_id,
        .mask = 0x1FFFFFFFU,
        .extended_id = false,
        .remote_frame = false,
    });

    boost::system::error_code ec;
    auto can_session = session::can::CanSession::create(io_context, config, &ec);
    if (ec) {
        std::cerr << "Error opening CAN interface: " << ec.message() << "\n";
        return 1;
    }

    benchmark::LatencyStats stats;
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
        auto frame = make_frame(options.can_id, expected_sequence, options.fd_frame);
        send_started_at = Clock::now();
        can_session->send(frame);
        ++sent_count;
    };

    can_session->set_read_handler([&](boost::system::error_code read_ec, session::can::CanFrame frame) {
        if (read_ec) {
            std::cerr << "CAN read error: " << read_ec.message() << "\n";
            failed = true;
            completed = true;
            can_session->close();
            io_context.stop();
            return;
        }

        if (frame.id != options.can_id || frame.data_length < sizeof(std::uint32_t)) {
            return;
        }

        std::uint32_t actual_sequence = 0;
        std::memcpy(&actual_sequence, frame.data.data(), sizeof(actual_sequence));
        if (actual_sequence != expected_sequence) {
            std::cerr << "CAN benchmark out-of-order loopback: expected sequence "
                      << expected_sequence << ", got " << actual_sequence << "\n";
            failed = true;
            completed = true;
            can_session->close();
            io_context.stop();
            return;
        }

        stats.add(std::chrono::duration_cast<benchmark::LatencyStats::Duration>(
            Clock::now() - send_started_at
        ));
        ++received_count;

        if (received_count >= options.samples) {
            completed = true;
            can_session->close();
            io_context.stop();
            return;
        }

        send_next();
    });

    can_session->start_reading();
    send_next();

    std::thread timeout_thread([&]() {
        const auto start = Clock::now();
        while (!completed.load()) {
            if (Clock::now() - start >= options.timeout * static_cast<int>(options.samples)) {
                std::cerr << "CAN benchmark timeout after " << received_count
                          << " completed samples\n";
                failed = true;
                completed = true;
                can_session->close();
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

    stats.print_summary("can_rtt");
    return 0;
}
