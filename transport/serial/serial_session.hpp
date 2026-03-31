#pragma once

#include "core/async_transport_session.hpp"
#include "transport/serial/serial_config.hpp"

#include <boost/asio/serial_port.hpp>

#include <memory>
#include <vector>

namespace session::serial {

class SerialSession:
    public AsyncTransportSession<SerialSession, std::vector<char>, std::vector<char>>,
    public std::enable_shared_from_this<SerialSession> {
public:
    using Base        = AsyncTransportSession<SerialSession, std::vector<char>, std::vector<char>>;
    using ReadHandler = Base::ReadHandler;
    using DuplexMode   = session::serial::DuplexMode;

    explicit SerialSession(boost::asio::io_context& io_context):
        Base(io_context),
        socket_(io_context) {}

    static std::shared_ptr<SerialSession> create(
        boost::asio::io_context& io_context,
        const SerialPortConfig& config,
        boost::system::error_code* ec = nullptr
    ) {
        auto session = std::make_shared<SerialSession>(io_context);
        if (ec) {
            *ec = session->open(config);
        } else {
            boost::system::error_code ignored_ec = session->open(config);
            (void)ignored_ec;
        }

        return session;
    }

    boost::system::error_code open(const SerialPortConfig& config) {
        boost::system::error_code ec;
        socket_.open(config.device_path, ec);
        if (ec) {
            return ec;
        }

        ec = apply_config(config);
        if (ec) {
            boost::system::error_code close_ec;
            socket_.close(close_ec);
        }

        return ec;
    }

    boost::system::error_code apply_config(const SerialPortConfig& config) {
        auto& port = socket_;

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

        duplex_mode_ = config.duplex_mode;
        read_buffer_.resize(config.read_buffer_size);
        return {};
    }

    bool is_open() const {
        return socket_.is_open();
    }

    boost::asio::serial_port& socket() {
        return socket_;
    }

    const boost::asio::serial_port& socket() const {
        return socket_;
    }

    void set_duplex_mode(DuplexMode duplex_mode) {
        boost::asio::post(strand(), [this, self = shared_from_this(), duplex_mode]() {
            duplex_mode_ = duplex_mode;
            schedule_operations();
        });
    }

    void set_read_buffer_size(std::size_t read_buffer_size) {
        boost::asio::post(strand(), [this, self = shared_from_this(), read_buffer_size]() {
            if (is_read_in_progress() || read_buffer_size == 0U) {
                return;
            }

            read_buffer_.resize(read_buffer_size);
        });
    }

    bool can_start_read() const {
        if (!is_read_enabled() || is_read_in_progress() || is_closed()) {
            return false;
        }

        if (duplex_mode_ == DuplexMode::half_duplex) {
            return !is_write_in_progress() && !has_pending_writes();
        }

        return true;
    }

    bool can_start_write() const {
        if (is_write_in_progress() || !has_pending_writes() || is_closed()) {
            return false;
        }

        if (duplex_mode_ == DuplexMode::half_duplex) {
            return !is_read_in_progress();
        }

        return true;
    }

    void do_read() {
        mark_read_started();

        socket_.async_read_some(
            boost::asio::buffer(read_buffer_),
            boost::asio::bind_executor(
                strand(),
                [this, self = shared_from_this()](boost::system::error_code ec, std::size_t bytes_transferred) {
                    mark_read_finished();

                    if (ec) {
                        notify_read(ec, {});
                        close();
                        return;
                    }

                    std::vector<char> data(
                        read_buffer_.begin(),
                        read_buffer_.begin() + static_cast<std::vector<char>::difference_type>(bytes_transferred)
                    );
                    notify_read({}, std::move(data));
                    schedule_operations();
                }
            )
        );
    }

    void do_write() {
        auto buffer = current_write();
        if (!buffer) {
            return;
        }

        mark_write_started();

        boost::asio::async_write(
            socket_,
            boost::asio::buffer(*buffer),
            boost::asio::bind_executor(
                strand(),
                [this, self = shared_from_this()](boost::system::error_code ec, std::size_t /*bytes_transferred*/) {
                    mark_write_finished();

                    if (ec) {
                        close();
                        return;
                    }

                    consume_write();
                    schedule_operations();
                }
            )
        );
    }

    void close_transport() {
        boost::system::error_code ec;
        socket_.close(ec);
    }

private:
    boost::asio::serial_port socket_;
    DuplexMode duplex_mode_ = DuplexMode::full_duplex;
    std::vector<char> read_buffer_ = std::vector<char>(default_read_buffer_size);
};

} // namespace session::serial
