#pragma once

#include "ysmparser/asset.hpp"

namespace omm::ysm {

std::unique_ptr<Asset> parse_current_asset(std::span<const uint8_t> input, const std::string &display_name,
                                           const Limits &limits);

} // namespace omm::ysm
