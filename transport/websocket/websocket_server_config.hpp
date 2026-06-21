#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace session::websocket {

/**
 * @brief WebSocket 服务端监听与会话配置。
 */
struct WebSocketServerConfig {
    /** @brief 监听地址。 */
    std::string bind_address = "127.0.0.1";
    /** @brief 监听端口。 */
    std::uint16_t port = 9002;

    /** @brief 是否以 binary message 模式发送。 */
    bool binary_mode = true;
    /** @brief 是否禁用 Nagle。 */
    bool tcp_no_delay = true;
    /** @brief 允许接收的最大 message 大小。 */
    std::size_t max_message_size = 1024 * 1024;
    /** @brief listen backlog。 */
    int backlog = 16;
};

} // namespace session::websocket
