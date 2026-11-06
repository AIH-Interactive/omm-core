// C++ consumer test: the public header compiles as C++ with identical layouts.
#include <cstddef>
#include <cstdio>
#include <type_traits>

#include "omm/omm.h"

static_assert(sizeof(omm_error) == 536, "omm_error layout");
static_assert(offsetof(omm_error, message) == 24, "omm_error.message");
static_assert(sizeof(omm_version) == 96, "omm_version layout");
static_assert(sizeof(omm_runtime_desc) == 48, "omm_runtime_desc layout");
static_assert(sizeof(omm_runtime_caps) == 32, "omm_runtime_caps layout");
static_assert(std::is_standard_layout<omm_runtime_desc>::value, "C-compatible layout");
static_assert(std::is_same<omm_handle, std::uint64_t>::value, "64-bit handles");

int main() {
    omm_version version{};
    version.struct_size = sizeof(version);
    if (omm_get_version(&version, nullptr) != omm_ok) {
        std::fprintf(stderr, "omm_get_version failed\n");
        return 1;
    }
    std::printf("header_cpp_test: ABI %u.%u build %s\n", version.abi_major, version.abi_minor,
                version.build_id);
    return 0;
}
