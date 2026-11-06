/*
 * omm-core public C ABI.
 *
 * Contract: docs/OmniMorph-native-api.md. Every public identifier starts with
 * omm_ / OMM_. The header is C11 and C++ compatible; no STL, exceptions, C++
 * bool or platform-width long cross this boundary.
 *
 * ABI major 0 is the pre-release development line (M1-M5); see
 * docs/m0/M0-contract.md section 4.
 */
#ifndef OMM_OMM_H
#define OMM_OMM_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#    if defined(OMM_BUILDING_LIBRARY)
#        define OMM_API __declspec(dllexport)
#    else
#        define OMM_API __declspec(dllimport)
#    endif
#    define OMM_CALL __cdecl
#else
#    if defined(OMM_BUILDING_LIBRARY)
#        define OMM_API __attribute__((visibility("default")))
#    else
#        define OMM_API
#    endif
#    define OMM_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define OMM_ABI_MAJOR 0u
#define OMM_ABI_MINOR 1u

/* Schema/cache versions are independent of the ABI version. */
#define OMM_ASSET_SCHEMA_VERSION 0u
#define OMM_MESH_SCHEMA_VERSION 0u
#define OMM_CACHE_VERSION 0u

/* Unknown source offset in omm_error. */
#define OMM_OFFSET_UNKNOWN UINT64_MAX

typedef int32_t omm_status;
typedef uint64_t omm_handle;
typedef uint32_t omm_cpu_mode;
typedef uint32_t omm_render_mode;
typedef uint32_t omm_backend;

/* Status codes. Values are part of the ABI and never reused. */
#define omm_ok 0
#define omm_error_argument 1
#define omm_error_abi 2
#define omm_error_handle 3
#define omm_error_unsupported 4
#define omm_error_format 5
#define omm_error_limit 6
#define omm_error_memory 7
#define omm_error_capacity 8
#define omm_error_thread 9
#define omm_error_device_lost 10
#define omm_error_shader 11
#define omm_error_state 12
#define omm_error_partial_submit 13
#define omm_error_internal 14

/* CPU kernel selection. */
#define omm_cpu_auto 0u
#define omm_cpu_scalar 1u
#define omm_cpu_sse2 2u
#define omm_cpu_avx2 3u
#define omm_cpu_neon 4u

/* Graphics backends (M0 contract section 4). 0 is invalid; 3 is reserved for Vulkan. */
#define omm_backend_gl 1u
#define omm_backend_gles 2u
#define omm_backend_vulkan_reserved 3u

/* Error stages: where a failure was detected. */
#define omm_stage_none 0u
#define omm_stage_abi 1u
#define omm_stage_runtime 2u
#define omm_stage_jni 3u

typedef struct omm_bytes {
    const uint8_t *data;
    uint64_t size;
} omm_bytes;

typedef struct omm_mut_bytes {
    uint8_t *data;
    uint64_t capacity;
} omm_mut_bytes;

typedef struct omm_error {
    uint32_t struct_size;
    int32_t code;
    uint32_t stage;
    uint32_t reserved;
    uint64_t source_offset;
    char message[512];
} omm_error;

#define OMM_VERSION_STRUCT_VERSION 1u

typedef struct omm_version {
    uint32_t struct_size;
    uint32_t version;
    uint32_t abi_major;
    uint32_t abi_minor;
    uint32_t asset_schema;
    uint32_t mesh_schema;
    uint32_t cache_version;
    uint32_t reserved;
    char build_id[64];
} omm_version;

#define OMM_RUNTIME_DESC_VERSION 1u

typedef struct omm_runtime_desc {
    uint32_t struct_size;
    uint32_t version;
    uint32_t requested_abi_major;
    uint32_t requested_abi_minor;
    omm_cpu_mode cpu_mode;
    uint32_t worker_count;
    uint64_t cpu_memory_budget;
    uint64_t parse_max_file_bytes;
    uint64_t parse_max_total_bytes;
} omm_runtime_desc;

#define OMM_RUNTIME_CAPS_VERSION 1u

/* Bit positions of omm_runtime_caps.cpu_kernels: (1u << omm_cpu_xxx). */
typedef struct omm_runtime_caps {
    uint32_t struct_size;
    uint32_t version;
    uint32_t cpu_kernels;
    omm_cpu_mode active_kernel;
    uint32_t parser_formats;
    uint32_t worker_count;
    uint32_t abi_major;
    uint32_t abi_minor;
} omm_runtime_caps;

OMM_API omm_status OMM_CALL omm_get_version(omm_version *out_version, omm_error *error);

OMM_API omm_status OMM_CALL omm_runtime_create(const omm_runtime_desc *desc,
                                               omm_handle *out_runtime,
                                               omm_error *error);

OMM_API omm_status OMM_CALL omm_runtime_get_caps(omm_handle runtime,
                                                 omm_runtime_caps *out_caps,
                                                 omm_error *error);

OMM_API omm_status OMM_CALL omm_runtime_destroy(omm_handle runtime, omm_error *error);

#ifdef __cplusplus
}
#endif

#endif /* OMM_OMM_H */
