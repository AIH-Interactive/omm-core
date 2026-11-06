#include "abi/cpu_features.hpp"

#if defined(_M_X64) || defined(__x86_64__)
#    define OMM_ARCH_X64 1
#    if defined(_MSC_VER)
#        include <intrin.h>
#        include <immintrin.h>
#    else
#        include <cpuid.h>
#    endif
#elif defined(_M_ARM64) || defined(__aarch64__)
#    define OMM_ARCH_ARM64 1
#endif

namespace omm::abi {

namespace {

constexpr uint32_t bit(omm_cpu_mode mode) noexcept {
    return 1u << mode;
}

#if defined(OMM_ARCH_X64)

void cpuid(int leaf, int subleaf, unsigned int out[4]) noexcept {
#    if defined(_MSC_VER)
    int registers[4];
    __cpuidex(registers, leaf, subleaf);
    for (int i = 0; i < 4; ++i) {
        out[i] = static_cast<unsigned int>(registers[i]);
    }
#    else
    __cpuid_count(leaf, subleaf, out[0], out[1], out[2], out[3]);
#    endif
}

uint64_t read_xcr0() noexcept {
#    if defined(_MSC_VER)
    return _xgetbv(0);
#    else
    unsigned int low = 0;
    unsigned int high = 0;
    __asm__ volatile("xgetbv" : "=a"(low), "=d"(high) : "c"(0));
    return (static_cast<uint64_t>(high) << 32) | low;
#    endif
}

// AVX2 needs the CPU flag, OSXSAVE, and the OS saving XMM+YMM state (XCR0 bits 1 and 2).
bool has_avx2() noexcept {
    unsigned int registers[4] = {};
    cpuid(0, 0, registers);
    if (registers[0] < 7u) {
        return false;
    }
    cpuid(1, 0, registers);
    const bool osxsave = (registers[2] & (1u << 27)) != 0u;
    const bool avx = (registers[2] & (1u << 28)) != 0u;
    if (!osxsave || !avx) {
        return false;
    }
    if ((read_xcr0() & 0x6u) != 0x6u) {
        return false;
    }
    cpuid(7, 0, registers);
    return (registers[1] & (1u << 5)) != 0u;
}

#endif

}  // namespace

uint32_t available_cpu_kernels() noexcept {
    uint32_t kernels = bit(omm_cpu_scalar);
#if defined(OMM_ARCH_X64)
    // SSE2 is part of the x86-64 baseline.
    kernels |= bit(omm_cpu_sse2);
    if (has_avx2()) {
        kernels |= bit(omm_cpu_avx2);
    }
#elif defined(OMM_ARCH_ARM64)
    // Advanced SIMD (NEON) is mandatory on AArch64.
    kernels |= bit(omm_cpu_neon);
#endif
    return kernels;
}

omm_cpu_mode best_cpu_kernel(uint32_t available) noexcept {
    if ((available & bit(omm_cpu_avx2)) != 0u) {
        return omm_cpu_avx2;
    }
    if ((available & bit(omm_cpu_neon)) != 0u) {
        return omm_cpu_neon;
    }
    if ((available & bit(omm_cpu_sse2)) != 0u) {
        return omm_cpu_sse2;
    }
    return omm_cpu_scalar;
}

}  // namespace omm::abi
