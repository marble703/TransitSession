#include "transport/websocket/websocket_server.hpp"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

struct Options {
    std::string bind_address = "127.0.0.1";
    std::uint16_t port = 9002;
    bool binary_mode = false;
};

bool parse_unsigned(const char* text, std::uint16_t& value) {
    try {
        const auto parsed = std::stoul(text);
        value = static_cast<std::uint16_t>(parsed);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool parse_options(int argc, char* argv[], Options& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--bind" && i + 1 < argc) {
            options.bind_address = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            if (!parse_unsigned(argv[++i], options.port)) {
                return false;
            }
        } else if (arg == "--binary") {
            options.binary_mode = true;
        } else if (arg == "--help") {
            return false;
        } else {
            return false;
        }
    }

    return true;
}

void print_usage() {
    std::cerr << "Usage: TransitSessionWebSocketServer [--bind <address>] [--port <port>] "
                 "[--binary]\n";
}

void attach_echo_behavior(const std::shared_ptr<session::websocket::WebSocketServerSession>& session) {
    session->set_read_handler(
        [session](boost::system::error_code ec, std::vector<char> data) mutable {
            if (ec) {
                if (ec != boost::beast::websocket::error::closed) {
                    std::cerr << "WebSocket session read error: " << ec.message() << "\n";
                }
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

    boost::asio::io_context io_context;
    session::websocket::WebSocketServerConfig config;
    config.bind_address = options.bind_address;
    config.port = options.port;
    config.binary_mode = options.binary_mode;

    boost::system::error_code ec;
    auto server = session::websocket::WebSocketServer::create(io_context, config, &ec);
    if (ec) {
        std::cerr << "Error starting WebSocket server: " << ec.message() << "\n";
        return 1;
    }

    server->set_session_handler(
        [&](boost::system::error_code session_ec,
            std::shared_ptr<session::websocket::WebSocketServerSession> session) {
            if (session_ec) {
                if (session_ec != boost::asio::error::operation_aborted) {
                    std::cerr << "WebSocket accept/handshake error: " << session_ec.message()
                              << "\n";
                }
                return;
            }

            std::cout << "Accepted WebSocket connection\n";
            attach_echo_behavior(session);
        }
    );

    server->start();

    try {
        io_context.run();
    } catch (const std::exception& ex) {
        std::cerr << "io_context.run() exception: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
