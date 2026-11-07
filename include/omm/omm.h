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
#define OMM_ABI_MINOR 3u

/* Schema/cache versions are independent of the ABI version. */
#define OMM_ASSET_SCHEMA_VERSION 1u
#define OMM_MESH_SCHEMA_VERSION 1u
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
#define omm_stage_parse 4u
#define omm_stage_model 5u

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
    /* Total CPU execution threads, including the caller. 0 = one per logical CPU core. */
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

/* ---------------------------------------------------------------------------
 * ysmparser (API section 4, ABI 0.2)
 * ------------------------------------------------------------------------- */

/* Source formats; bit (1u << format) in omm_runtime_caps.parser_formats and
 * omm_ysm_parse_desc.allowed_formats. */
#define omm_ysm_unknown 0u
#define omm_ysm_raw 1u
#define omm_ysm_zip 2u
#define omm_ysm_legacy_v1 3u
#define omm_ysm_legacy_v2 4u
#define omm_ysm_legacy_v3 5u
#define omm_ysm_asset_container 6u

/* Source semantics profile. 0 selects the profile from the detected format. */
#define omm_profile_omp_compat 1u
#define omm_profile_ysm_current 2u

/* Asset sections (UTF-8 JSON, schema_version 1). */
#define omm_section_metadata 1u
#define omm_section_animations 2u
#define omm_section_controllers 3u
#define omm_section_expressions 4u
#define omm_section_resources 5u
#define omm_section_diagnostics 6u

#define OMM_YSM_PROBE_RESULT_VERSION 1u

typedef struct omm_ysm_probe_result {
    uint32_t struct_size;
    uint32_t version;
    uint32_t format;          /* omm_ysm_* */
    uint32_t source_version;  /* YSGP crypto version, else 0 */
    uint32_t parseable;       /* 1 when this build has a decoder for the format */
    uint32_t reserved;
    uint64_t min_input_bytes; /* bytes needed before the format can be decided */
} omm_ysm_probe_result;

/* One raw-directory file; path is a UTF-8 relative path. Fixed leaf struct. */
typedef struct omm_ysm_entry {
    omm_bytes path;
    omm_bytes data;
} omm_ysm_entry;

#define OMM_YSM_PARSE_DESC_VERSION 1u

/* Limits: 0 selects the runtime default (never "unlimited"). */
typedef struct omm_ysm_parse_desc {
    uint32_t struct_size;
    uint32_t version;
    uint32_t source_profile;
    uint32_t allowed_formats; /* 0 = every format compiled in */
    omm_bytes display_name;   /* fallback model name, UTF-8; may be empty */
    uint64_t input_bytes_limit;
    uint64_t expanded_bytes_limit;
    uint64_t entry_bytes_limit;
    uint32_t entry_count_limit;
    uint32_t nesting_limit;
    uint32_t bone_limit;
    uint32_t vertex_limit;
    uint32_t keyframe_limit;
    uint32_t reserved;
} omm_ysm_parse_desc;

#define OMM_ASSET_INFO_VERSION 1u

typedef struct omm_asset_info {
    uint32_t struct_size;
    uint32_t version;
    uint32_t source_format;  /* omm_ysm_* */
    uint32_t source_version; /* crypto version (YSGP) or binary format (v3 payload) */
    uint32_t asset_schema;
    uint32_t geometry_count;
    uint32_t bone_count;
    uint32_t quad_count;
    uint32_t animation_file_count;
    uint32_t clip_count;
    uint32_t controller_count;
    uint32_t resource_count;
    uint32_t diagnostic_count;
    uint32_t reserved;
    uint64_t retained_bytes;
} omm_asset_info;

OMM_API omm_status OMM_CALL omm_ysm_probe(omm_bytes input, omm_ysm_probe_result *out_result,
                                          omm_error *error);

OMM_API omm_status OMM_CALL omm_ysm_parse(omm_handle runtime, omm_bytes input,
                                          const omm_ysm_parse_desc *desc,
                                          omm_handle *out_asset, omm_error *error);

OMM_API omm_status OMM_CALL omm_ysm_parse_entries(omm_handle runtime, const omm_ysm_entry *entries,
                                                  uint32_t count, const omm_ysm_parse_desc *desc,
                                                  omm_handle *out_asset, omm_error *error);

OMM_API omm_status OMM_CALL omm_asset_get_info(omm_handle asset, omm_asset_info *out_info,
                                               omm_error *error);

/* Empty output (capacity 0) queries *out_required; insufficient capacity returns
 * omm_error_capacity without writing. */
OMM_API omm_status OMM_CALL omm_asset_read_section(omm_handle asset, uint32_t section,
                                                   omm_mut_bytes output, uint64_t *out_required,
                                                   omm_error *error);

/* Chunked read; offset at or past the end succeeds with *out_written = 0. */
OMM_API omm_status OMM_CALL omm_asset_read_resource(omm_handle asset, uint32_t resource_id,
                                                    uint64_t offset, omm_mut_bytes output,
                                                    uint64_t *out_written, omm_error *error);

OMM_API omm_status OMM_CALL omm_asset_release(omm_handle asset, omm_error *error);

/* Model inputs: Figura uses pixel units (1/16 block); YSM is already normalized to blocks.
 * Bind translations follow the input profile. Pose updates always use block units. */
#define omm_geometry_figura 1u
#define omm_geometry_ysm 2u
#define OMM_PART_ROOT UINT32_MAX
#define OMM_MATERIAL_INHERIT UINT32_MAX
#define omm_surface_alpha_unknown 1u
#define omm_surface_translucent 2u
#define omm_surface_double_sided 4u
#define omm_surface_front_cull 8u

typedef struct omm_part_desc {
    uint32_t parent_id;
    uint32_t reserved;
    double local_bind[16];
} omm_part_desc;

typedef struct omm_vertex {
    float position[3];
    float normal[3];
    float uv[2];
    uint32_t part_id;
} omm_vertex;

typedef struct omm_surface_desc {
    uint32_t first_index;
    uint32_t index_count;
    uint32_t material_id;
    uint32_t flags;
    uint64_t ordinal;
} omm_surface_desc;

/* Texture slots and semantic IDs are host-owned identifiers, never native GL names. */
typedef struct omm_material_desc {
    uint32_t semantic_id;
    uint32_t primary_texture;
    uint32_t secondary_texture;
    uint32_t flags;
    float alpha;
    float emissive;
    uint32_t light;
    uint32_t overlay;
} omm_material_desc;

#define OMM_MODEL_DESC_VERSION 1u
typedef struct omm_model_desc {
    uint32_t struct_size;
    uint32_t version;
    uint32_t geometry_profile;
    uint32_t part_count;
    const omm_part_desc *parts;
    uint32_t vertex_count;
    uint32_t index_count;
    const omm_vertex *vertices;
    const uint32_t *indices;
    uint32_t surface_count;
    uint32_t material_count;
    const omm_surface_desc *surfaces;
    const omm_material_desc *materials;
} omm_model_desc;

#define OMM_BAKE_DESC_VERSION 1u
#define omm_alpha_invisible 0u
#define omm_alpha_opaque 1u
#define omm_alpha_translucent 2u
#define omm_alpha_unknown 3u
typedef struct omm_bake_desc {
    uint32_t struct_size;
    uint32_t version;
    uint32_t geometry_id;
    uint32_t geometry_profile; /* 0 or omm_geometry_ysm */
    const uint8_t *quad_alpha; /* optional explicit classification in asset quad order */
    uint64_t quad_alpha_count;
    uint64_t memory_budget; /* 0 = runtime budget; never exceeds the runtime budget */
} omm_bake_desc;

#define OMM_POSE_UPDATE_VERSION 1u
#define omm_pose_full 1u
#define omm_pose_delta 2u
typedef struct omm_pose_update {
    uint32_t struct_size;
    uint32_t version;
    uint64_t revision;
    uint32_t mode;
    uint32_t part_count;
    const uint32_t *part_ids;
    const double *local_matrices; /* part_count * 16, required unless count is zero */
    const uint32_t *visibility;   /* optional, values 0/1 */
    const float *colors;         /* optional, part_count * 4 */
    const double *uv_matrices;   /* optional, part_count * 9 */
    const uint32_t *materials;   /* optional, IDs or OMM_MATERIAL_INHERIT */
} omm_pose_update;

/* Fixed leaf output. Query validates every requested ID before writing any result. */
typedef struct omm_part_result {
    uint64_t revision;
    double model_matrix[16];
    double normal_matrix[9];
    double uv_matrix[9];
    float color[4];
    uint32_t visible;
    uint32_t material_id;
} omm_part_result;

#define OMM_MODEL_INFO_VERSION 1u
typedef struct omm_model_info {
    uint32_t struct_size;
    uint32_t version;
    uint32_t geometry_profile;
    uint32_t part_count;
    uint32_t vertex_count;
    uint32_t index_count;
    uint32_t surface_count;
    uint32_t material_count;
} omm_model_info;

OMM_API omm_status OMM_CALL omm_model_create(omm_handle runtime, const omm_model_desc *desc,
                                            omm_handle *out_model, omm_error *error);
OMM_API omm_status OMM_CALL omm_model_create_from_asset(omm_handle runtime, omm_handle asset,
                                                       const omm_bake_desc *desc, omm_handle *out_model,
                                                       omm_error *error);
OMM_API omm_status OMM_CALL omm_model_release(omm_handle model, omm_error *error);
OMM_API omm_status OMM_CALL omm_model_get_info(omm_handle model, omm_model_info *out_info, omm_error *error);
OMM_API omm_status OMM_CALL omm_instance_create(omm_handle model, omm_handle *out_instance, omm_error *error);
OMM_API omm_status OMM_CALL omm_instance_update(omm_handle instance, const omm_pose_update *update, omm_error *error);
OMM_API omm_status OMM_CALL omm_instance_query_parts(omm_handle instance, const uint32_t *part_ids, uint32_t count,
                                                    omm_part_result *out_parts, omm_error *error);
OMM_API omm_status OMM_CALL omm_instance_release(omm_handle instance, omm_error *error);

/* Synchronous CPU transform of one immutable pose snapshot. Empty output queries vertex count.
 * No graphics calls; insufficient capacity/failure leaves output vertices untouched. */
OMM_API omm_status OMM_CALL omm_instance_transform_cpu(omm_handle instance, omm_cpu_mode kernel,
                                                       omm_vertex *output, uint32_t capacity,
                                                       uint32_t *out_required, uint64_t *out_revision,
                                                       omm_error *error);

typedef struct omm_sort_item {
    double center[3];
    uint64_t ordinal;
} omm_sort_item;

/* Sort only one host-approved semantic segment. Equal keys preserve input order. */
OMM_API omm_status OMM_CALL omm_sort_back_to_front(omm_handle runtime, const omm_sort_item *items, uint32_t count,
                                                  const double camera[3], uint32_t *out_order, omm_error *error);

#ifdef __cplusplus
}
#endif

#endif /* OMM_OMM_H */
