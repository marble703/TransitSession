#pragma once

#include <array>
#include <cstdint>
#include <cstddef>

namespace session::can {

struct CanFrame {
    static constexpr std::size_t classic_data_size = 8;
    static constexpr std::size_t fd_data_size = 64;

    std::uint32_t id = 0;
    std::size_t data_length = 0;
    std::array<std::uint8_t, fd_data_size> data{};

    bool extended_id = false;
    bool remote_frame = false;
    bool error_frame = false;
    bool fd_frame = false;
    bool bitrate_switch = false;
    bool error_state_indicator = false;
};

} // namespace session::can
