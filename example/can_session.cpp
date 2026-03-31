#include "transport/can/can_session.hpp"

#include <iostream>

int main() {
    boost::asio::io_context io_context;

    session::can::CanConfig config {};
    config.interface_name = "can0";
    config.enable_can_fd = true;

    boost::system::error_code ec;
    auto session = session::can::CanSession::create(io_context, config, &ec);
    if (ec) {
        std::cerr << "Error opening CAN interface: " << ec.message() << "\n";
        return 0;
    }

    session->set_read_handler([](boost::system::error_code read_ec, session::can::CanFrame frame) {
        if (read_ec) {
            std::cerr << "CAN read error: " << read_ec.message() << "\n";
            return;
        }

        std::cout << "CAN frame received: id=0x" << std::hex << frame.id << std::dec
                  << ", len=" << frame.data_length
                  << ", fd=" << (frame.fd_frame ? "yes" : "no") << "\n";
    });

    return 0;
}
