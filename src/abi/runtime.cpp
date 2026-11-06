#include "abi/runtime.hpp"

#include <algorithm>
#include <cstring>
#include <memory>
#include <thread>

#include "abi/cpu_features.hpp"
#include "abi/error.hpp"

#ifndef OMM_BUILD_ID
#    define OMM_BUILD_ID "unknown"
#endif

namespace omm::abi {

namespace {

constexpr uint32_t kMaxWorkers = 64;
constexpr uint32_t kDefaultWorkerCap = 8;
constexpr uint64_t kDefaultMemoryBudget = 1024ull * 1024ull * 1024ull;  // 1 GiB
constexpr uint64_t kDefaultMaxFileBytes = 256ull * 1024ull * 1024ull;   // 256 MiB
constexpr uint64_t kDefaultMaxTotalBytes = 512ull * 1024ull * 1024ull;  // 512 MiB

// Output structs: the caller declares its size; v1 layouts must be complete.
template <typename Struct>
void require_output(Struct *out, uint32_t stage, const char *what) {
    if (out == nullptr) {
        throw Failure(omm_error_argument, stage, what);
    }
    if (out->struct_size < sizeof(Struct)) {
        throw Failure(omm_error_argument, stage, "output struct_size is smaller than the v1 layout");
    }
}

uint32_t default_worker_count() noexcept {
    const unsigned int hardware = std::thread::hardware_concurrency();
    if (hardware == 0u) {
        return 1u;
    }
    return std::min<uint32_t>(static_cast<uint32_t>(hardware), kDefaultWorkerCap);
}

}  // namespace

HandleTable<Runtime> &runtime_table() {
    static HandleTable<Runtime> table(HandleKind::runtime);
    return table;
}

}  // namespace omm::abi

using omm::abi::Failure;
using omm::abi::guard;

extern "C" {

OMM_API omm_status OMM_CALL omm_get_version(omm_version *out_version, omm_error *error) {
    return guard(error, omm_stage_abi, [&]() -> omm_status {
        omm::abi::require_output(out_version, omm_stage_abi, "out_version is null");
        const uint32_t declared_size = out_version->struct_size;
        std::memset(out_version, 0, sizeof(omm_version));
        out_version->struct_size = declared_size;
        out_version->version = OMM_VERSION_STRUCT_VERSION;
        out_version->abi_major = OMM_ABI_MAJOR;
        out_version->abi_minor = OMM_ABI_MINOR;
        out_version->asset_schema = OMM_ASSET_SCHEMA_VERSION;
        out_version->mesh_schema = OMM_MESH_SCHEMA_VERSION;
        out_version->cache_version = OMM_CACHE_VERSION;
        // build_id was zeroed above, so a bounded copy stays NUL-terminated.
        const size_t build_id_length =
            std::min(std::strlen(OMM_BUILD_ID), sizeof(out_version->build_id) - 1);
        std::memcpy(out_version->build_id, OMM_BUILD_ID, build_id_length);
        return omm_ok;
    });
}

OMM_API omm_status OMM_CALL omm_runtime_create(const omm_runtime_desc *desc,
                                               omm_handle *out_runtime,
                                               omm_error *error) {
    if (out_runtime != nullptr) {
        *out_runtime = 0;
    }
    return guard(error, omm_stage_runtime, [&]() -> omm_status {
        if (desc == nullptr || out_runtime == nullptr) {
            throw Failure(omm_error_argument, omm_stage_runtime, "desc or out_runtime is null");
        }
        if (desc->struct_size < sizeof(omm_runtime_desc) || desc->version < OMM_RUNTIME_DESC_VERSION) {
            throw Failure(omm_error_argument, omm_stage_runtime, "omm_runtime_desc size/version is invalid");
        }
        if (desc->requested_abi_major != OMM_ABI_MAJOR) {
            throw Failure(omm_error_abi, omm_stage_abi, "requested ABI major is not provided by this library");
        }
        if (desc->requested_abi_minor > OMM_ABI_MINOR) {
            throw Failure(omm_error_abi, omm_stage_abi, "requested ABI minor is newer than this library");
        }
        if (desc->cpu_mode > omm_cpu_neon) {
            throw Failure(omm_error_argument, omm_stage_runtime, "cpu_mode is not a known omm_cpu value");
        }
        if (desc->worker_count > omm::abi::kMaxWorkers) {
            throw Failure(omm_error_argument, omm_stage_runtime, "worker_count exceeds the supported maximum");
        }

        auto runtime = std::make_shared<omm::abi::Runtime>();
        runtime->available_kernels = omm::abi::available_cpu_kernels();
        if (desc->cpu_mode == omm_cpu_auto) {
            runtime->active_kernel = omm::abi::best_cpu_kernel(runtime->available_kernels);
        } else if ((runtime->available_kernels & (1u << desc->cpu_mode)) != 0u) {
            runtime->active_kernel = desc->cpu_mode;
        } else {
            // Explicit modes never silently degrade (API section 3).
            throw Failure(omm_error_unsupported, omm_stage_runtime, "requested cpu_mode is not available");
        }
        runtime->worker_count = desc->worker_count == 0u ? omm::abi::default_worker_count() : desc->worker_count;
        runtime->cpu_memory_budget = desc->cpu_memory_budget == 0u ? omm::abi::kDefaultMemoryBudget : desc->cpu_memory_budget;
        runtime->parse_max_file_bytes =
            desc->parse_max_file_bytes == 0u ? omm::abi::kDefaultMaxFileBytes : desc->parse_max_file_bytes;
        runtime->parse_max_total_bytes =
            desc->parse_max_total_bytes == 0u ? omm::abi::kDefaultMaxTotalBytes : desc->parse_max_total_bytes;

        *out_runtime = omm::abi::runtime_table().insert(std::move(runtime));
        return omm_ok;
    });
}

OMM_API omm_status OMM_CALL omm_runtime_get_caps(omm_handle runtime_handle,
                                                 omm_runtime_caps *out_caps,
                                                 omm_error *error) {
    return guard(error, omm_stage_runtime, [&]() -> omm_status {
        omm::abi::require_output(out_caps, omm_stage_runtime, "out_caps is null");
        const auto runtime = omm::abi::runtime_table().get(runtime_handle, omm_stage_runtime);
        const uint32_t declared_size = out_caps->struct_size;
        std::memset(out_caps, 0, sizeof(omm_runtime_caps));
        out_caps->struct_size = declared_size;
        out_caps->version = OMM_RUNTIME_CAPS_VERSION;
        out_caps->cpu_kernels = runtime->available_kernels;
        out_caps->active_kernel = runtime->active_kernel;
        // No parser formats are compiled in before M2.
        out_caps->parser_formats = 0u;
        out_caps->worker_count = runtime->worker_count;
        out_caps->abi_major = OMM_ABI_MAJOR;
        out_caps->abi_minor = OMM_ABI_MINOR;
        return omm_ok;
    });
}

OMM_API omm_status OMM_CALL omm_runtime_destroy(omm_handle runtime_handle, omm_error *error) {
    return guard(error, omm_stage_runtime, [&]() -> omm_status {
        auto &table = omm::abi::runtime_table();
        const auto runtime = table.get(runtime_handle, omm_stage_runtime);
        if (runtime->live_children.load() != 0u) {
            throw Failure(omm_error_state, omm_stage_runtime, "runtime still owns live child objects");
        }
        table.remove(runtime_handle, omm_stage_runtime);
        return omm_ok;
    });
}

}  // extern "C"
