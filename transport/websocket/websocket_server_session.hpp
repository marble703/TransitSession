#pragma once

#include "core/async_transport_session.hpp"
#include "transport/websocket/websocket_server_config.hpp"

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/websocket.hpp>

#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace session::websocket {

/**
 * @brief WebSocket 服务端会话。
 */
class WebSocketServerSession:
    public AsyncTransportSession<WebSocketServerSession, std::vector<char>, std::vector<char>>,
    public std::enable_shared_from_this<WebSocketServerSession> {
public:
    using Base = AsyncTransportSession<WebSocketServerSession, std::vector<char>, std::vector<char>>;
    using ReadHandler = Base::ReadHandler;
    using Socket = boost::beast::websocket::stream<boost::asio::ip::tcp::socket>;
    using StartHandler = std::function<void(boost::system::error_code)>;
    using CloseHandler = std::function<void()>;

    static std::shared_ptr<WebSocketServerSession> create(
        boost::asio::io_context& io_context,
        boost::asio::ip::tcp::socket socket,
        const WebSocketServerConfig& config
    ) {
        return std::shared_ptr<WebSocketServerSession>(
            new WebSocketServerSession(io_context, std::move(socket), config)
        );
    }

    void start(StartHandler handler) {
        boost::asio::dispatch(
            strand(),
            [this, self = shared_from_this(), handler = std::move(handler)]() mutable {
                boost::system::error_code ec;
                if (config_.tcp_no_delay) {
                    socket_.next_layer().set_option(boost::asio::ip::tcp::no_delay(true), ec);
                    if (ec) {
                        close_transport();
                        if (handler) {
                            handler(ec);
                        }
                        return;
                    }
                }

                socket_.read_message_max(config_.max_message_size);
                socket_.binary(config_.binary_mode);
                socket_.set_option(
                    boost::beast::websocket::stream_base::timeout::suggested(
                        boost::beast::role_type::server
                    )
                );

                socket_.async_accept(boost::asio::bind_executor(
                    strand(),
                    [this, self = shared_from_this(), handler = std::move(handler)](
                        boost::system::error_code accept_ec
                    ) mutable {
                        if (accept_ec) {
                            close_transport();
                        }

                        if (handler) {
                            handler(accept_ec);
                        }
                    }
                ));
            }
        );
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

    void set_close_handler(CloseHandler handler) {
        boost::asio::dispatch(
            strand(),
            [this, self = shared_from_this(), handler = std::move(handler)]() mutable {
                close_handler_ = std::move(handler);
            }
        );
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
        if (close_notified_) {
            return;
        }

        close_notified_ = true;

        boost::system::error_code ec;
        socket_.next_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        socket_.next_layer().close(ec);

        if (close_handler_) {
            close_handler_();
        }
    }

private:
    WebSocketServerSession(
        boost::asio::io_context& io_context,
        boost::asio::ip::tcp::socket socket,
        const WebSocketServerConfig& config
    ):
        Base(io_context),
        socket_(std::move(socket)),
        config_(config) {}

    Socket socket_;
    boost::beast::flat_buffer read_buffer_;
    WebSocketServerConfig config_;
    CloseHandler close_handler_;
    bool close_notified_ = false;
};

} // namespace session::websocket
