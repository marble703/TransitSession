#pragma once

#include "core/async_session.hpp"

#include <boost/asio.hpp>

#include <string>

namespace session::serial {
using Parity = boost::asio::serial_port_base::parity;
using StopBits = boost::asio::serial_port_base::stop_bits;
using FlowControl = boost::asio::serial_port_base::flow_control;

struct SerialPortConfig {
    std::string device_path = "/dev/ttyACM0";

    unsigned int baud_rate      = 115200;
    unsigned int character_size = 8;
    Parity::type parity       = Parity::none;
    StopBits::type stop_bits  = StopBits::one;
    FlowControl::type flow_control = FlowControl::none;

    AsyncSession<boost::asio::serial_port>::DuplexMode duplex_mode =
        AsyncSession<boost::asio::serial_port>::DuplexMode::full_duplex;
    std::size_t read_buffer_size = AsyncSession<boost::asio::serial_port>::default_read_buffer_size;
};
} // namespace session::serial
