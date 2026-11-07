#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "abi/runtime.hpp"

namespace omm::model {

// The host/preparer defines sortable primitive boundaries; this does not merge surfaces
// across semantic segments, material changes, callbacks or host draw barriers.
struct SortItem {
    std::array<double, 3> center;
    uint64_t ordinal;
};

std::vector<uint32_t> sort_back_to_front(std::span<const SortItem> items, const std::array<double, 3> &camera,
                                          uint32_t threads, abi::CpuExecutor *executor = nullptr);

} // namespace omm::model
