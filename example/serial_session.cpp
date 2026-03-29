#include "transport/serial/serial_session.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>

int main() {
    boost::asio::io_context io_context;

    session::serial::SerialPortConfig config {
        .device_path      = "/dev/ttyACM0",
        .baud_rate        = 115200,
        .character_size   = 8,
        .parity           = boost::asio::serial_port_base::parity::none,
        .stop_bits        = boost::asio::serial_port_base::stop_bits::one,
        .flow_control     = boost::asio::serial_port_base::flow_control::none,
        .duplex_mode      = session::serial::SerialSession::DuplexMode::full_duplex,
        .read_buffer_size = 1024,
    };

    auto [session, ec] = session::serial::SerialSession::create(io_context, config);
    if (ec) {
        std::cerr << "Error opening serial port: " << ec.message() << "\n";
        return 1;
    }

    session->set_read_handler([](boost::system::error_code /*ec*/, std::vector<char> data) {
        std::cout << "Received data (" << data.size() << " bytes): ";

        for (char byte: data) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte)
                      << " ";
        }
        std::cout << std::dec << std::endl;

        std::cout << "Timestamp: " << std::chrono::steady_clock::now().time_since_epoch().count()
                  << std::endl;
    });
    session->start_reading();
    try {
        io_context.run();
    } catch (const std::exception& ex) {
        std::cerr << "io_context.run() exception: " << ex.what() << "\n";
    }
    return 0;
}
