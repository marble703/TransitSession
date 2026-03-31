#pragma once

#include <array>
#include <cstdint>
#include <cstddef>

namespace session::can {

/**
 * @brief CAN 帧描述，兼容经典 CAN 与 CAN FD。
 */
struct CanFrame {
    /** @brief 经典 CAN 最大有效载荷。 */
    static constexpr std::size_t classic_data_size = 8;
    /** @brief CAN FD 最大有效载荷。 */
    static constexpr std::size_t fd_data_size = 64;

    /** @brief 帧 ID。 */
    std::uint32_t id = 0;
    /** @brief 实际数据长度。 */
    std::size_t data_length = 0;
    /** @brief 帧数据区。 */
    std::array<std::uint8_t, fd_data_size> data{};

    /** @brief 是否为扩展帧。 */
    bool extended_id = false;
    /** @brief 是否为远程帧。 */
    bool remote_frame = false;
    /** @brief 是否为错误帧。 */
    bool error_frame = false;
    /** @brief 是否为 CAN FD 帧。 */
    bool fd_frame = false;
    /** @brief CAN FD bit rate switch。 */
    bool bitrate_switch = false;
    /** @brief CAN FD error state indicator。 */
    bool error_state_indicator = false;
};

} // namespace session::can
