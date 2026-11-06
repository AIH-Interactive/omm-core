// Runtime CPU feature detection for kernel selection.
#pragma once

#include <cstdint>

#include "omm/omm.h"

namespace omm::abi {

// Bitmap of (1u << omm_cpu_xxx) for kernels compiled in AND supported by this CPU/OS.
uint32_t available_cpu_kernels() noexcept;

// Best kernel from the available bitmap.
omm_cpu_mode best_cpu_kernel(uint32_t available) noexcept;

}  // namespace omm::abi
