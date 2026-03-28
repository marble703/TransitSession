#include "session/async_session.hpp"

#include <boost/asio.hpp>
#include <boost/asio/serial_port.hpp>

#include <memory>

int main() {
    boost::asio::io_context io_context;
    auto session =
        std::make_shared<AsyncSession<boost::asio::serial_port>>(io_context, "/dev/ttyUSB0");

    session->socket().set_option(boost::asio::serial_port_base::baud_rate(9600));
    session->socket().set_option(boost::asio::serial_port_base::character_size(8));
    session->socket().set_option(
        boost::asio::serial_port_base::parity(boost::asio::serial_port_base::parity::none)
    );
    session->socket().set_option(
        boost::asio::serial_port_base::stop_bits(boost::asio::serial_port_base::stop_bits::one)
    );

    session->set_duplex_mode(AsyncSession<boost::asio::serial_port>::DuplexMode::full_duplex);
    session->set_read_handler([](boost::system::error_code /*ec*/, std::vector<char> /*data*/) {});
    session->start_reading();

    return 0;
}
