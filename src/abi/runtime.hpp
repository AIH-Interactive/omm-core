// Runtime object: the root of every handle family.
#pragma once

#include <atomic>
#include <cstdint>

#include "abi/handle_table.hpp"
#include "omm/omm.h"

namespace omm::abi {

class CpuExecutor;

struct Runtime {
    omm_cpu_mode active_kernel = omm_cpu_scalar;
    uint32_t available_kernels = 0;
    uint32_t worker_count = 1;
    std::shared_ptr<CpuExecutor> cpu_executor;
    uint64_t cpu_memory_budget = 0;
    uint64_t parse_max_file_bytes = 0;
    uint64_t parse_max_total_bytes = 0;
    std::atomic<uint64_t> retained_cpu_bytes{0};
    // Live child objects (assets, models, devices ...); destroy fails with omm_error_state while > 0.
    std::atomic<uint64_t> live_children{0};
};

HandleTable<Runtime> &runtime_table();

}  // namespace omm::abi
