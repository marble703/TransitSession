#pragma once

#include <cstddef>
#include <string>

namespace session::websocket {

/**
 * @brief WebSocket 客户端连接配置。
 */
struct WebSocketConfig {
    /** @brief 目标主机名或 IP。 */
    std::string host = "127.0.0.1";
    /** @brief 目标端口。 */
    std::string port = "9002";
    /** @brief WebSocket 请求目标。 */
    std::string target = "/";

    /** @brief 是否以 binary message 模式发送。 */
    bool binary_mode = true;
    /** @brief 是否禁用 Nagle。 */
    bool tcp_no_delay = true;
    /** @brief 允许接收的最大 message 大小。 */
    std::size_t max_message_size = 1024 * 1024;
};

} // namespace session::websocket
