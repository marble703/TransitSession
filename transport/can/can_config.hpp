#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace session::can {

/**
 * @brief CAN 接收过滤规则。
 */
struct CanFilter {
    /** @brief 过滤器 ID。 */
    std::uint32_t id = 0;
    /** @brief 过滤器掩码。 */
    std::uint32_t mask = 0;
    /** @brief 是否匹配扩展帧。 */
    bool extended_id = false;
    /** @brief 是否匹配远程帧。 */
    bool remote_frame = false;
};

/**
 * @brief CAN 连接配置。
 */
struct CanConfig {
    /** @brief CAN 接口名，例如 `can0`。 */
    std::string interface_name = "can0";
    /** @brief 是否启用 CAN FD。 */
    bool enable_can_fd = true;
    /** @brief 是否开启回环。 */
    bool loopback = true;
    /** @brief 是否接收自己发送的帧。 */
    bool receive_own_messages = false;
    /** @brief 过滤器列表。 */
    std::vector<CanFilter> filters;
};

} // namespace session::can
