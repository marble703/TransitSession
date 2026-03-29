#pragma once

#include "core/async_session.hpp"

#include <boost/asio.hpp>

#include <string>

namespace session::serial {
struct SerialPortConfig {
    std::string device_path = "/dev/ttyACM0";

    unsigned int baud_rate      = 115200;
    unsigned int character_size = 8;
    boost::asio::serial_port_base::parity::type parity =
        boost::asio::serial_port_base::parity::none;
    boost::asio::serial_port_base::stop_bits::type stop_bits =
        boost::asio::serial_port_base::stop_bits::one;
    boost::asio::serial_port_base::flow_control::type flow_control =
        boost::asio::serial_port_base::flow_control::none;

    AsyncSession<boost::asio::serial_port>::DuplexMode duplex_mode =
        AsyncSession<boost::asio::serial_port>::DuplexMode::full_duplex;
    std::size_t read_buffer_size = AsyncSession<boost::asio::serial_port>::default_read_buffer_size;
};
} // namespace session::serial
