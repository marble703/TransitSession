#pragma once

#include "core/async_transport_session.hpp"
#include "transport/can/can_config.hpp"
#include "transport/can/can_frame.hpp"

#include <boost/asio/posix/stream_descriptor.hpp>

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

namespace session::can {

/**
 * @brief Linux SocketCAN 会话，提供帧级异步收发。
 */
class CanSession:
    public AsyncTransportSession<CanSession, CanFrame, CanFrame>,
    public std::enable_shared_from_this<CanSession> {
public:
    /** @brief 读取回调类型。 */
    using Base        = AsyncTransportSession<CanSession, CanFrame, CanFrame>;
    using ReadHandler = Base::ReadHandler;

    /**
     * @brief 构造一个 CAN 会话。
     * @param io_context Boost.Asio 运行上下文。
     */
    explicit CanSession(boost::asio::io_context& io_context):
        Base(io_context),
        socket_(io_context) {}

    /**
     * @brief 创建并打开 CAN 会话。
     * @param io_context Boost.Asio 运行上下文。
     * @param config CAN 配置。
     * @param ec 若提供，则返回打开结果。
     */
    static std::shared_ptr<CanSession> create(
        boost::asio::io_context& io_context,
        const CanConfig& config,
        boost::system::error_code* ec = nullptr
    ) {
        auto session = std::make_shared<CanSession>(io_context);
        if (ec) {
            *ec = session->open(config);
        } else {
            boost::system::error_code ignored_ec = session->open(config);
            (void)ignored_ec;
        }

        return session;
    }

    /**
     * @brief 打开 CAN 接口并配置 SocketCAN 选项。
     */
    boost::system::error_code open(const CanConfig& config) {
        if (is_open()) {
            boost::system::error_code already_open_ec =
                boost::system::errc::make_error_code(boost::system::errc::device_or_resource_busy);
            return already_open_ec;
        }

        const int fd = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
        if (fd < 0) {
            return last_system_error();
        }

        auto close_fd = [fd]() {
            ::close(fd);
        };

        auto ec = configure_socket(fd, config);
        if (ec) {
            close_fd();
            return ec;
        }

        const unsigned int ifindex = ::if_nametoindex(config.interface_name.c_str());
        if (ifindex == 0U) {
            close_fd();
            return last_system_error();
        }

        sockaddr_can addr {};
        addr.can_family  = AF_CAN;
        addr.can_ifindex  = static_cast<int>(ifindex);

        if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            close_fd();
            return last_system_error();
        }

        try {
            socket_.assign(fd);
        } catch (const boost::system::system_error& ex) {
            close_fd();
            return ex.code();
        }

        config_ = config;
        read_buffer_.fill(0);
        return {};
    }

    /** @brief 判断 CAN 套接字是否已打开。 */
    bool is_open() const {
        return socket_.is_open();
    }

    /** @brief 获取底层流描述符。 */
    boost::asio::posix::stream_descriptor& socket() {
        return socket_;
    }

    /** @brief 获取底层流描述符（只读）。 */
    const boost::asio::posix::stream_descriptor& socket() const {
        return socket_;
    }

    /** @brief 判断是否允许启动下一次读取。 */
    bool can_start_read() const {
        return is_read_enabled() && !is_read_in_progress() && !is_closed();
    }

    /** @brief 判断是否允许启动下一次写入。 */
    bool can_start_write() const {
        return is_write_in_progress() == false && has_pending_writes() && !is_closed();
    }

    /** @brief 启动一次异步 CAN 帧读取。 */
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

                    auto frame_ec = decode_frame(bytes_transferred, read_buffer_);
                    if (frame_ec) {
                        notify_read(frame_ec, {});
                        close();
                        return;
                    }

                    notify_read({}, last_read_frame_);
                    schedule_operations();
                }
            )
        );
    }

    /** @brief 启动一次异步 CAN 帧写入。 */
    void do_write() {
        auto frame = current_write();
        if (!frame) {
            return;
        }

        auto encoded = encode_frame(*frame);
        if (!encoded) {
            close();
            return;
        }

        mark_write_started();

        boost::asio::async_write(
            socket_,
            boost::asio::buffer(*encoded),
            boost::asio::bind_executor(
                strand(),
                [this, self = shared_from_this(), encoded](boost::system::error_code ec, std::size_t /*bytes_transferred*/) {
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

    /** @brief 关闭底层 CAN 资源。 */
    void close_transport() {
        try {
            socket_.close();
        } catch (const boost::system::system_error&) {
        }
    }

private:
    static boost::system::error_code last_system_error() {
        return {errno, boost::system::system_category()};
    }

    boost::system::error_code configure_socket(int fd, const CanConfig& config) {
        if (config.enable_can_fd) {
            int enable = 1;
            if (::setsockopt(fd, SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &enable, sizeof(enable)) < 0) {
                return last_system_error();
            }
        }

        {
            int loopback = config.loopback ? 1 : 0;
            if (::setsockopt(fd, SOL_CAN_RAW, CAN_RAW_LOOPBACK, &loopback, sizeof(loopback)) < 0) {
                return last_system_error();
            }
        }

        {
            int recv_own = config.receive_own_messages ? 1 : 0;
            if (::setsockopt(fd, SOL_CAN_RAW, CAN_RAW_RECV_OWN_MSGS, &recv_own, sizeof(recv_own)) < 0) {
                return last_system_error();
            }
        }

        if (!config.filters.empty()) {
            std::vector<can_filter> raw_filters;
            raw_filters.reserve(config.filters.size());

            for (const auto& filter: config.filters) {
                can_filter raw_filter {};
                raw_filter.can_id = filter.id;
                raw_filter.can_mask = filter.mask;

                if (filter.extended_id) {
                    raw_filter.can_id |= CAN_EFF_FLAG;
                    raw_filter.can_mask |= CAN_EFF_FLAG;
                }

                if (filter.remote_frame) {
                    raw_filter.can_id |= CAN_RTR_FLAG;
                    raw_filter.can_mask |= CAN_RTR_FLAG;
                }

                raw_filters.push_back(raw_filter);
            }

            if (::setsockopt(
                    fd,
                    SOL_CAN_RAW,
                    CAN_RAW_FILTER,
                    raw_filters.data(),
                    static_cast<socklen_t>(raw_filters.size() * sizeof(can_filter))
                ) < 0) {
                return last_system_error();
            }
        }

        return {};
    }

    boost::system::error_code decode_frame(std::size_t bytes_transferred, const std::array<std::uint8_t, sizeof(canfd_frame)>& raw_buffer) {
        if (bytes_transferred == sizeof(can_frame)) {
            can_frame raw {};
            std::memcpy(&raw, raw_buffer.data(), sizeof(can_frame));
            last_read_frame_ = decode_classic_frame(raw);
            return {};
        }

        if (bytes_transferred == sizeof(canfd_frame)) {
            canfd_frame raw {};
            std::memcpy(&raw, raw_buffer.data(), sizeof(canfd_frame));
            last_read_frame_ = decode_fd_frame(raw);
            return {};
        }

        return boost::system::errc::make_error_code(boost::system::errc::protocol_error);
    }

    static CanFrame decode_classic_frame(const can_frame& raw) {
        CanFrame frame;
        frame.id = raw.can_id & CAN_EFF_MASK;
        frame.extended_id = (raw.can_id & CAN_EFF_FLAG) != 0U;
        frame.remote_frame = (raw.can_id & CAN_RTR_FLAG) != 0U;
        frame.error_frame = (raw.can_id & CAN_ERR_FLAG) != 0U;
        frame.fd_frame = false;
        frame.data_length = raw.can_dlc;
        frame.data.fill(0);
        for (std::size_t i = 0; i < frame.data_length && i < CanFrame::classic_data_size; ++i) {
            frame.data[i] = raw.data[i];
        }
        return frame;
    }

    static CanFrame decode_fd_frame(const canfd_frame& raw) {
        CanFrame frame;
        frame.id = raw.can_id & CAN_EFF_MASK;
        frame.extended_id = (raw.can_id & CAN_EFF_FLAG) != 0U;
        frame.remote_frame = (raw.can_id & CAN_RTR_FLAG) != 0U;
        frame.error_frame = (raw.can_id & CAN_ERR_FLAG) != 0U;
        frame.fd_frame = true;
        frame.bitrate_switch = (raw.flags & CANFD_BRS) != 0U;
        frame.error_state_indicator = (raw.flags & CANFD_ESI) != 0U;
        frame.data_length = raw.len;
        frame.data.fill(0);
        for (std::size_t i = 0; i < frame.data_length && i < CanFrame::fd_data_size; ++i) {
            frame.data[i] = raw.data[i];
        }
        return frame;
    }

    std::shared_ptr<std::vector<std::uint8_t>> encode_frame(const CanFrame& frame) {
        if (frame.fd_frame) {
            if (!config_.enable_can_fd || frame.data_length > CanFrame::fd_data_size) {
                return {};
            }

            auto buffer = std::make_shared<std::vector<std::uint8_t>>(sizeof(canfd_frame));
            canfd_frame raw {};
            raw.can_id = frame.id;
            if (frame.extended_id) {
                raw.can_id |= CAN_EFF_FLAG;
            }
            if (frame.remote_frame) {
                raw.can_id |= CAN_RTR_FLAG;
            }
            raw.len = static_cast<__u8>(frame.data_length);
            raw.flags = 0;
            if (frame.bitrate_switch) {
                raw.flags |= CANFD_BRS;
            }
            if (frame.error_state_indicator) {
                raw.flags |= CANFD_ESI;
            }
            for (std::size_t i = 0; i < frame.data_length; ++i) {
                raw.data[i] = frame.data[i];
            }
            std::memcpy(buffer->data(), &raw, sizeof(canfd_frame));
            return buffer;
        }

        if (frame.data_length > CanFrame::classic_data_size) {
            return {};
        }

        auto buffer = std::make_shared<std::vector<std::uint8_t>>(sizeof(can_frame));
        can_frame raw {};
        raw.can_id = frame.id;
        if (frame.extended_id) {
            raw.can_id |= CAN_EFF_FLAG;
        }
        if (frame.remote_frame) {
            raw.can_id |= CAN_RTR_FLAG;
        }
        raw.can_dlc = static_cast<__u8>(frame.data_length);
        for (std::size_t i = 0; i < frame.data_length; ++i) {
            raw.data[i] = frame.data[i];
        }
        std::memcpy(buffer->data(), &raw, sizeof(can_frame));
        return buffer;
    }

    boost::asio::posix::stream_descriptor socket_;
    CanConfig config_;
    std::array<std::uint8_t, sizeof(canfd_frame)> read_buffer_ {};
    CanFrame last_read_frame_ {};
};

} // namespace session::can
