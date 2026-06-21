#include "example/latency_stats.hpp"
#include "transport/websocket/websocket_server.hpp"
#include "transport/websocket/websocket_session.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct Options {
    std::string host = "127.0.0.1";
    std::string target = "/";
    std::string listen_address = "127.0.0.1";
    std::uint16_t port = 9002;
    std::size_t payload_size = 32;
    std::size_t samples = 1000;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(1000);
    bool binary_mode = true;
    bool external_server = false;
};

bool parse_unsigned(const char* text, std::size_t& value) {
    try {
        value = std::stoull(text, nullptr, 0);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool parse_unsigned(const char* text, std::uint16_t& value) {
    try {
        const auto parsed = std::stoul(text, nullptr, 0);
        value = static_cast<std::uint16_t>(parsed);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool parse_options(int argc, char* argv[], Options& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) {
            options.host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            if (!parse_unsigned(argv[++i], options.port)) {
                return false;
            }
        } else if (arg == "--target" && i + 1 < argc) {
            options.target = argv[++i];
        } else if (arg == "--listen-address" && i + 1 < argc) {
            options.listen_address = argv[++i];
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
        } else if (arg == "--text") {
            options.binary_mode = false;
        } else if (arg == "--external-server") {
            options.external_server = true;
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
        << "Usage: TransitSessionWebSocketBenchmark [--host <hostname>] [--port <port>] "
        << "[--target </path>] [--listen-address <address>] [--payload <bytes>] "
        << "[--samples <count>] [--timeout-ms <ms>] [--text] [--external-server]\n"
        << "By default the benchmark starts a local echo server in-process.\n";
}

std::vector<char> make_payload(std::size_t payload_size, std::uint32_t sequence) {
    std::vector<char> payload(payload_size, 0);
    std::memcpy(payload.data(), &sequence, sizeof(sequence));
    for (std::size_t i = sizeof(sequence); i < payload.size(); ++i) {
        payload[i] = static_cast<char>(i & 0xffU);
    }
    return payload;
}

void attach_echo_behavior(const std::shared_ptr<session::websocket::WebSocketServerSession>& session) {
    session->set_read_handler(
        [session](boost::system::error_code ec, std::vector<char> data) mutable {
            if (ec) {
                session->close();
                return;
            }

            session->send(std::move(data));
        }
    );
    session->start_reading();
}

} // namespace

int main(int argc, char* argv[]) {
    Options options;
    if (!parse_options(argc, argv, options)) {
        print_usage();
        return 1;
    }

    boost::asio::io_context server_io_context;
    std::shared_ptr<session::websocket::WebSocketServer> server;
    std::thread server_thread;

    if (!options.external_server) {
        session::websocket::WebSocketServerConfig server_config;
        server_config.bind_address = options.listen_address;
        server_config.port = options.port;
        server_config.binary_mode = options.binary_mode;

        boost::system::error_code server_ec;
        server = session::websocket::WebSocketServer::create(server_io_context, server_config, &server_ec);
        if (server_ec) {
            std::cerr << "Error starting benchmark WebSocket server: " << server_ec.message()
                      << "\n";
            return 1;
        }

        server->set_session_handler(
            [](boost::system::error_code session_ec,
               std::shared_ptr<session::websocket::WebSocketServerSession> session) {
                if (session_ec || !session) {
                    return;
                }

                attach_echo_behavior(session);
            }
        );
        server->start();
        server_thread = std::thread([&]() { server_io_context.run(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    boost::asio::io_context client_io_context;
    session::websocket::WebSocketConfig client_config;
    client_config.host = options.host;
    client_config.port = std::to_string(options.port);
    client_config.target = options.target;
    client_config.binary_mode = options.binary_mode;
    client_config.max_message_size = options.payload_size * 4U;

    boost::system::error_code ec;
    auto client_session =
        session::websocket::WebSocketSession::create(client_io_context, client_config, &ec);
    if (ec) {
        std::cerr << "Error opening WebSocket connection: " << ec.message() << "\n";
        if (server) {
            server->stop();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            server_io_context.stop();
            server_thread.join();
        }
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
        auto payload = make_payload(options.payload_size, expected_sequence);
        send_started_at = Clock::now();
        client_session->send(std::move(payload));
        ++sent_count;
    };

    client_session->set_read_handler(
        [&](boost::system::error_code read_ec, std::vector<char> data) {
            if (read_ec) {
                std::cerr << "WebSocket read error: " << read_ec.message() << "\n";
                failed = true;
                completed = true;
                client_session->close();
                client_io_context.stop();
                return;
            }

            if (data.size() < sizeof(std::uint32_t)) {
                std::cerr << "WebSocket benchmark received undersized message\n";
                failed = true;
                completed = true;
                client_session->close();
                client_io_context.stop();
                return;
            }

            std::uint32_t actual_sequence = 0;
            std::memcpy(&actual_sequence, data.data(), sizeof(actual_sequence));
            if (actual_sequence != expected_sequence) {
                std::cerr << "WebSocket benchmark out-of-order echo: expected sequence "
                          << expected_sequence << ", got " << actual_sequence << "\n";
                failed = true;
                completed = true;
                client_session->close();
                client_io_context.stop();
                return;
            }

            stats.add(std::chrono::duration_cast<benchmark::LatencyStats::Duration>(
                Clock::now() - send_started_at
            ));
            ++received_count;

            if (received_count >= options.samples) {
                completed = true;
                client_session->close();
                client_io_context.stop();
                return;
            }

            send_next();
        }
    );

    client_session->start_reading();
    send_next();

    std::thread timeout_thread([&]() {
        const auto start = Clock::now();
        while (!completed.load()) {
            if (Clock::now() - start >= options.timeout * static_cast<int>(options.samples)) {
                std::cerr << "WebSocket benchmark timeout after " << received_count
                          << " completed samples\n";
                failed = true;
                completed = true;
                client_session->close();
                client_io_context.stop();
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    client_io_context.run();
    completed = true;
    timeout_thread.join();

    if (server) {
        server->stop();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        server_io_context.stop();
        server_thread.join();
    }

    if (failed.load()) {
        return 1;
    }

    stats.print_summary("websocket_rtt");
    return 0;
}
