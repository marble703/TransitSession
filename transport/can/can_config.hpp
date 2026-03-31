#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace session::can {

struct CanFilter {
    std::uint32_t id = 0;
    std::uint32_t mask = 0;
    bool extended_id = false;
    bool remote_frame = false;
};

struct CanConfig {
    std::string interface_name = "can0";
    bool enable_can_fd = true;
    bool loopback = true;
    bool receive_own_messages = false;
    std::vector<CanFilter> filters;
};

} // namespace session::can
