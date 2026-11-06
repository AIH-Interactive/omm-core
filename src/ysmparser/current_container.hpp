// Current YSM chunk container (AssetContainerConstant 0.1.0-unstable).
#pragma once

#include <array>
#include <map>

#include "ysmparser/resource_set.hpp"

namespace omm::ysm {

struct CurrentContainer {
    std::string schema;
    std::map<uint16_t, std::string> properties;
    std::map<std::string, std::string> encodings;
    std::array<uint8_t, 32> container_id{};
    ResourceSet chunks;
};

bool is_current_container(std::span<const uint8_t> input) noexcept;
CurrentContainer decode_current_container(std::span<const uint8_t> input, Budget &budget);

} // namespace omm::ysm
