#pragma once

#include "core/async_session.hpp"
#include "transport/serial/serial_config.hpp"

#include <boost/asio/serial_port.hpp>

namespace session::serial {

class SerialSession: public AsyncSession<boost::asio::serial_port> {
public:
    using AsyncSession<boost::asio::serial_port>::AsyncSession;
    using DuplexMode = AsyncSession<boost::asio::serial_port>::DuplexMode;

    static std::shared_ptr<SerialSession> create(
        boost::asio::io_context& io_context,
        const SerialPortConfig& config,
        boost::system::error_code* ec = nullptr
    ) {
        auto session = std::make_shared<SerialSession>(io_context);
        *ec          = session->open(config);

        return session;
    }

    boost::system::error_code open(const SerialPortConfig& config) {
        auto ec = AsyncSession<boost::asio::serial_port>::open(config.device_path);
        if (ec) {
            return ec;
        }

        ec = apply_config(config);
        if (ec) {
            boost::system::error_code close_ec;
            socket().close(close_ec);
        }

        return ec;
    }

    boost::system::error_code apply_config(const SerialPortConfig& config) {
        auto& port = socket();

        boost::system::error_code ec;
        port.set_option(boost::asio::serial_port_base::baud_rate(config.baud_rate), ec);
        if (ec) {
            return ec;
        }

        port.set_option(boost::asio::serial_port_base::character_size(config.character_size), ec);
        if (ec) {
            return ec;
        }

        port.set_option(boost::asio::serial_port_base::parity(config.parity), ec);
        if (ec) {
            return ec;
        }

        port.set_option(boost::asio::serial_port_base::stop_bits(config.stop_bits), ec);
        if (ec) {
            return ec;
        }

        port.set_option(boost::asio::serial_port_base::flow_control(config.flow_control), ec);
        if (ec) {
            return ec;
        }

        set_duplex_mode(config.duplex_mode);
        set_read_buffer_size(config.read_buffer_size);
        return {};
    }
};

} // namespace session::serial
