#include "core/async_session.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>

int main() {
    boost::asio::io_context io_context;
    auto session = std::make_shared<AsyncSession<boost::asio::serial_port>>(io_context);
    auto ec      = session->open("/dev/ttyACM0");
    if (ec) {
        std::cerr << "Error opening serial port: " << ec.message() << "\n";
        return 1;
    }

    session->socket().set_option(boost::asio::serial_port_base::baud_rate(9600));
    session->socket().set_option(boost::asio::serial_port_base::character_size(8));
    session->socket().set_option(
        boost::asio::serial_port_base::parity(boost::asio::serial_port_base::parity::none)
    );
    session->socket().set_option(
        boost::asio::serial_port_base::stop_bits(boost::asio::serial_port_base::stop_bits::one)
    );

    session->set_duplex_mode(AsyncSession<boost::asio::serial_port>::DuplexMode::full_duplex);
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
