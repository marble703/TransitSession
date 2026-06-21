#pragma once

#include "transport/websocket/websocket_server_config.hpp"
#include "transport/websocket/websocket_server_session.hpp"

#include <boost/asio.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <functional>
#include <memory>
#include <unordered_set>

namespace session::websocket {

/**
 * @brief WebSocket 服务端监听器。
 */
class WebSocketServer:
    public std::enable_shared_from_this<WebSocketServer> {
public:
    using SessionHandler =
        std::function<void(boost::system::error_code, std::shared_ptr<WebSocketServerSession>)>;

    static std::shared_ptr<WebSocketServer> create(
        boost::asio::io_context& io_context,
        const WebSocketServerConfig& config,
        boost::system::error_code* ec = nullptr
    ) {
        auto server = std::shared_ptr<WebSocketServer>(new WebSocketServer(io_context, config));
        auto open_ec = server->open();
        if (ec) {
            *ec = open_ec;
        }

        if (open_ec) {
            return {};
        }

        return server;
    }

    void set_session_handler(SessionHandler handler) {
        boost::asio::dispatch(
            strand_,
            [this, self = shared_from_this(), handler = std::move(handler)]() mutable {
                session_handler_ = std::move(handler);
            }
        );
    }

    void start() {
        boost::asio::dispatch(strand_, [this, self = shared_from_this()]() {
            if (started_) {
                return;
            }

            started_ = true;
            do_accept();
        });
    }

    void stop() {
        boost::asio::dispatch(strand_, [this, self = shared_from_this()]() {
            started_ = false;
            boost::system::error_code ec;
            acceptor_.cancel(ec);
            acceptor_.close(ec);

            for (const auto& session: active_sessions_) {
                session->close();
            }
        });
    }

private:
    WebSocketServer(boost::asio::io_context& io_context, const WebSocketServerConfig& config):
        io_context_(io_context),
        strand_(boost::asio::make_strand(io_context)),
        acceptor_(io_context),
        config_(config) {}

    boost::system::error_code open() {
        boost::system::error_code ec;
        const auto address = boost::asio::ip::make_address(config_.bind_address, ec);
        if (ec) {
            return ec;
        }

        boost::asio::ip::tcp::endpoint endpoint(address, config_.port);
        acceptor_.open(endpoint.protocol(), ec);
        if (ec) {
            return ec;
        }

        acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
        if (ec) {
            boost::system::error_code close_ec;
            acceptor_.close(close_ec);
            return ec;
        }

        acceptor_.bind(endpoint, ec);
        if (ec) {
            boost::system::error_code close_ec;
            acceptor_.close(close_ec);
            return ec;
        }

        acceptor_.listen(config_.backlog, ec);
        if (ec) {
            boost::system::error_code close_ec;
            acceptor_.close(close_ec);
        }
        return ec;
    }

    void do_accept() {
        acceptor_.async_accept(boost::asio::bind_executor(
            strand_,
            [this, self = shared_from_this()](
                boost::system::error_code ec,
                boost::asio::ip::tcp::socket socket
            ) {
                if (!started_) {
                    return;
                }

                if (ec) {
                    if (session_handler_) {
                        session_handler_(ec, {});
                    }
                    if (ec != boost::asio::error::operation_aborted) {
                        do_accept();
                    }
                    return;
                }

                auto session = WebSocketServerSession::create(io_context_, std::move(socket), config_);
                session->set_close_handler([this, self = shared_from_this(), session]() {
                    boost::asio::dispatch(strand_, [this, self, session]() {
                        active_sessions_.erase(session);
                    });
                });
                session->start(
                    [this, self = shared_from_this(), session](boost::system::error_code start_ec) {
                        boost::asio::dispatch(
                            strand_,
                            [this, self, session, start_ec]() {
                                if (!start_ec) {
                                    active_sessions_.insert(session);
                                }
                                if (session_handler_) {
                                    session_handler_(start_ec, start_ec ? nullptr : session);
                                }
                            }
                        );
                    }
                );

                do_accept();
            }
        ));
    }

    boost::asio::io_context& io_context_;
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
    boost::asio::ip::tcp::acceptor acceptor_;
    WebSocketServerConfig config_;
    SessionHandler session_handler_;
    std::unordered_set<std::shared_ptr<WebSocketServerSession>> active_sessions_;
    bool started_ = false;
};

} // namespace session::websocket
