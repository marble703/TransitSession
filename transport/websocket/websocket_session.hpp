#pragma once

#include "core/async_transport_session.hpp"
#include "transport/websocket/websocket_config.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/socket_base.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/websocket.hpp>

#include <memory>
#include <string>
#include <vector>

namespace session::websocket {

/**
 * @brief 基于 Boost.Beast 的 WebSocket 客户端会话。
 */
class WebSocketSession:
    public AsyncTransportSession<WebSocketSession, std::vector<char>, std::vector<char>>,
    public std::enable_shared_from_this<WebSocketSession> {
public:
    using Base = AsyncTransportSession<WebSocketSession, std::vector<char>, std::vector<char>>;
    using ReadHandler = Base::ReadHandler;
    using Socket = boost::beast::websocket::stream<boost::asio::ip::tcp::socket>;

    /**
     * @brief 创建并打开 WebSocket 会话。
     */
    static std::shared_ptr<WebSocketSession> create(
        boost::asio::io_context& io_context,
        const WebSocketConfig& config,
        boost::system::error_code* ec = nullptr
    ) {
        auto session = std::shared_ptr<WebSocketSession>(new WebSocketSession(io_context));
        auto open_ec = session->open(config);
        if (ec) {
            *ec = open_ec;
        }

        if (open_ec) {
            return {};
        }

        return session;
    }

    /**
     * @brief 同步建立 TCP 连接并完成 WebSocket 握手。
     */
    boost::system::error_code open(const WebSocketConfig& config) {
        if (is_open()) {
            return boost::system::errc::make_error_code(
                boost::system::errc::device_or_resource_busy
            );
        }

        boost::system::error_code ec;
        auto endpoints = resolver_.resolve(config.host, config.port, ec);
        if (ec) {
            return ec;
        }

        boost::asio::connect(socket_.next_layer(), endpoints, ec);
        if (ec) {
            return ec;
        }

        if (config.tcp_no_delay) {
            socket_.next_layer().set_option(
                boost::asio::ip::tcp::no_delay(true),
                ec
            );
            if (ec) {
                boost::system::error_code close_ec;
                socket_.next_layer().close(close_ec);
                return ec;
            }
        }

        socket_.read_message_max(config.max_message_size);
        socket_.binary(config.binary_mode);
        socket_.set_option(
            boost::beast::websocket::stream_base::timeout::suggested(
                boost::beast::role_type::client
            )
        );

        auto host_header = config.host;
        if (!config.port.empty()) {
            host_header += ":" + config.port;
        }

        socket_.handshake(host_header, config.target, ec);
        if (ec) {
            boost::system::error_code close_ec;
            socket_.next_layer().close(close_ec);
            return ec;
        }

        config_ = config;
        return {};
    }

    bool is_open() const {
        return socket_.next_layer().is_open();
    }

    Socket& socket() {
        return socket_;
    }

    const Socket& socket() const {
        return socket_;
    }

    bool can_start_read() const {
        return is_read_enabled() && !is_read_in_progress() && !is_closed();
    }

    bool can_start_write() const {
        return !is_write_in_progress() && has_pending_writes() && !is_closed();
    }

    void do_read() {
        mark_read_started();

        read_buffer_.consume(read_buffer_.size());
        socket_.async_read(
            read_buffer_,
            boost::asio::bind_executor(
                strand(),
                [this, self = shared_from_this()](
                    boost::system::error_code ec,
                    std::size_t bytes_transferred
                ) {
                    mark_read_finished();

                    if (ec) {
                        notify_read(ec, {});
                        close();
                        return;
                    }

                    std::vector<char> message(bytes_transferred);
                    boost::asio::buffer_copy(boost::asio::buffer(message), read_buffer_.data());
                    read_buffer_.consume(bytes_transferred);

                    notify_read({}, std::move(message));
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
        socket_.binary(config_.binary_mode);
        auto write_buffer = buffer;
        const auto asio_buffer = boost::asio::buffer(*write_buffer);
        socket_.async_write(
            asio_buffer,
            boost::asio::bind_executor(
                strand(),
                [this, self = shared_from_this(), write_buffer = std::move(write_buffer)](
                    boost::system::error_code ec,
                    std::size_t /*bytes_transferred*/
                ) {
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
        socket_.next_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        socket_.next_layer().close(ec);
    }

private:
    explicit WebSocketSession(boost::asio::io_context& io_context):
        Base(io_context),
        resolver_(io_context),
        socket_(io_context) {}

    boost::asio::ip::tcp::resolver resolver_;
    Socket socket_;
    boost::beast::flat_buffer read_buffer_;
    WebSocketConfig config_;
};

} // namespace session::websocket
