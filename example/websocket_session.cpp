#include "transport/websocket/websocket_session.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace {

struct Options {
    std::string host = "127.0.0.1";
    std::string port = "9002";
    std::string target = "/";
    std::string message = "hello from TransitSession";
};

bool parse_options(int argc, char* argv[], Options& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) {
            options.host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            options.port = argv[++i];
        } else if (arg == "--target" && i + 1 < argc) {
            options.target = argv[++i];
        } else if (arg == "--message" && i + 1 < argc) {
            options.message = argv[++i];
        } else if (arg == "--help") {
            return false;
        } else {
            return false;
        }
    }

    return true;
}

void print_usage() {
    std::cerr << "Usage: TransitSessionWebSocket --host <hostname> --port <port> "
                 "[--target </path>] [--message <text>]\n";
}

} // namespace

int main(int argc, char* argv[]) {
    Options options;
    if (!parse_options(argc, argv, options)) {
        print_usage();
        return 1;
    }

    boost::asio::io_context io_context;

    session::websocket::WebSocketConfig config;
    config.host = options.host;
    config.port = options.port;
    config.target = options.target;
    config.binary_mode = false;

    boost::system::error_code ec;
    auto websocket_session = session::websocket::WebSocketSession::create(io_context, config, &ec);
    if (ec) {
        std::cerr << "Error opening WebSocket connection: " << ec.message() << "\n";
        return 1;
    }

    websocket_session->set_read_handler(
        [&](boost::system::error_code read_ec, std::vector<char> data) {
            if (read_ec) {
                std::cerr << "WebSocket read error: " << read_ec.message() << "\n";
                io_context.stop();
                return;
            }

            std::string text(data.begin(), data.end());
            std::cout << "Received message: " << text << "\n";
            websocket_session->close();
            io_context.stop();
        }
    );

    websocket_session->start_reading();
    websocket_session->send(std::vector<char>(options.message.begin(), options.message.end()));

    try {
        io_context.run();
    } catch (const std::exception& ex) {
        std::cerr << "io_context.run() exception: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
