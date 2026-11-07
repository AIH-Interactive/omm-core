// C ABI for the native ysmparser (omm_ysm_* / omm_asset_*).
#include <algorithm>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "abi/error.hpp"
#include "abi/handle_table.hpp"
#include "abi/runtime.hpp"
#include "ysmparser/asset_memory.hpp"
#include "ysmparser/access.hpp"
#include "ysmparser/json.hpp"
#include "ysmparser/parse.hpp"
#include "ysmparser/ysgp.hpp"

namespace omm::ysm {

namespace {

constexpr uint32_t kSupportedFormats = (1u << omm_ysm_raw) | (1u << omm_ysm_zip) | (1u << omm_ysm_legacy_v1) |
                                       (1u << omm_ysm_legacy_v2) | (1u << omm_ysm_legacy_v3) |
                                       (1u << omm_ysm_asset_container);

// Asset handle payload. Holding the runtime keeps live_children accounting exact.
struct AssetObject {
    std::shared_ptr<abi::Runtime> runtime;
    std::unique_ptr<Asset> asset;
    std::mutex section_mutex;
    std::unordered_map<uint32_t, std::string> sections; // serialized once, then reused
    uint64_t accounted_bytes = 0;

    void reserve_memory(uint64_t bytes) {
        uint64_t current = runtime->retained_cpu_bytes.load();
        do {
            if (bytes > runtime->cpu_memory_budget || current > runtime->cpu_memory_budget - bytes) {
                fail(DecodeCode::resource_limit, "memory/runtime", 0);
            }
        } while (!runtime->retained_cpu_bytes.compare_exchange_weak(current, current + bytes));
        accounted_bytes += bytes;
    }

    ~AssetObject() {
        if (runtime) {
            runtime->retained_cpu_bytes.fetch_sub(accounted_bytes);
            runtime->live_children.fetch_sub(1);
        }
    }
};

abi::HandleTable<AssetObject> &asset_table() {
    static abi::HandleTable<AssetObject> table(abi::HandleKind::asset);
    return table;
}

uint32_t format_code(SourceFormat format) {
    switch (format) {
    case SourceFormat::raw:
        return omm_ysm_raw;
    case SourceFormat::zip:
        return omm_ysm_zip;
    case SourceFormat::legacy_v1:
        return omm_ysm_legacy_v1;
    case SourceFormat::legacy_v2:
        return omm_ysm_legacy_v2;
    case SourceFormat::legacy_v3:
        return omm_ysm_legacy_v3;
    case SourceFormat::asset_container:
        return omm_ysm_asset_container;
    case SourceFormat::unknown:
        break;
    }
    return omm_ysm_unknown;
}

uint32_t container_format(const Asset &asset) {
    switch (asset.source.container) {
    case SourceContainer::directory:
        return omm_ysm_raw;
    case SourceContainer::zip:
        return omm_ysm_zip;
    case SourceContainer::legacy_v1:
        return omm_ysm_legacy_v1;
    case SourceContainer::legacy_v2:
        return omm_ysm_legacy_v2;
    case SourceContainer::v3:
        return omm_ysm_legacy_v3;
    case SourceContainer::current:
        return omm_ysm_asset_container;
    }
    return omm_ysm_unknown;
}

void require_bytes(omm_bytes bytes, const char *what) {
    if (bytes.data == nullptr && bytes.size != 0) {
        throw abi::Failure(omm_error_argument, omm_stage_parse, std::string(what) + " has null data");
    }
}

struct ParseSettings {
    Limits limits;
    uint32_t allowed_formats = kSupportedFormats;
    uint32_t nesting_limit = 0;
    uint32_t bone_limit = 0;
    uint32_t vertex_limit = 0;
    uint32_t keyframe_limit = 0;
    std::string display_name;
    uint32_t source_profile = 0;
};

void constrain_decode_memory(ParseSettings &settings, const abi::Runtime &runtime) {
    settings.limits.file_bytes = std::min(settings.limits.file_bytes, runtime.cpu_memory_budget);
    settings.limits.single_resource_bytes = std::min(settings.limits.single_resource_bytes, runtime.cpu_memory_budget);
    settings.limits.total_resource_bytes = std::min(settings.limits.total_resource_bytes, runtime.cpu_memory_budget);
}

ParseSettings settings_from(const abi::Runtime &runtime, const omm_ysm_parse_desc *desc) {
    ParseSettings settings;
    settings.limits.worker_count = runtime.worker_count;
    settings.limits.file_bytes = runtime.parse_max_file_bytes;
    settings.limits.total_resource_bytes = runtime.parse_max_total_bytes;
    if (desc == nullptr) {
        constrain_decode_memory(settings, runtime);
        return settings;
    }
    if (desc->struct_size < sizeof(omm_ysm_parse_desc) || desc->version < OMM_YSM_PARSE_DESC_VERSION) {
        throw abi::Failure(omm_error_argument, omm_stage_parse, "omm_ysm_parse_desc size/version is invalid");
    }
    if (desc->source_profile != 0 && desc->source_profile != omm_profile_omp_compat &&
        desc->source_profile != omm_profile_ysm_current) {
        throw abi::Failure(omm_error_argument, omm_stage_parse, "unknown source_profile");
    }
    settings.source_profile = desc->source_profile;
    if (desc->allowed_formats != 0) {
        settings.allowed_formats = desc->allowed_formats;
    }
    require_bytes(desc->display_name, "display_name");
    settings.display_name.assign(reinterpret_cast<const char *>(desc->display_name.data),
                                 static_cast<size_t>(desc->display_name.size));
    if (!is_valid_utf8(settings.display_name)) {
        throw abi::Failure(omm_error_argument, omm_stage_parse, "display_name is not valid UTF-8");
    }
    if (desc->input_bytes_limit != 0)
        settings.limits.file_bytes = desc->input_bytes_limit;
    if (desc->expanded_bytes_limit != 0)
        settings.limits.total_resource_bytes = desc->expanded_bytes_limit;
    if (desc->entry_bytes_limit != 0)
        settings.limits.single_resource_bytes = desc->entry_bytes_limit;
    if (desc->entry_count_limit != 0)
        settings.limits.resource_count = desc->entry_count_limit;
    settings.nesting_limit = desc->nesting_limit;
    settings.limits.nesting_limit = desc->nesting_limit;
    settings.bone_limit = desc->bone_limit;
    settings.vertex_limit = desc->vertex_limit;
    settings.keyframe_limit = desc->keyframe_limit;
    constrain_decode_memory(settings, runtime);
    return settings;
}

// Post-parse model-size limits (the decoders already enforce the reference's fixed caps).
void enforce_model_limits(const Asset &asset, const ParseSettings &settings) {
    uint64_t bones = 0;
    uint64_t vertices = 0;
    for (const Geometry &geometry : asset.geometries) {
        bones += geometry.bones.size();
        for (const GeometryBone &bone : geometry.bones) {
            for (const GeometryCube &cube : bone.cubes) {
                vertices += cube.quads.size() * 4u;
            }
        }
    }
    uint64_t keyframes = 0;
    for (const AnimationFile &file : asset.animation_files) {
        for (const AnimationClip &clip : file.clips) {
            for (const BoneAnimation &bone : clip.bones) {
                for (const auto *channel : {&bone.position, &bone.rotation, &bone.scale}) {
                    if (*channel) {
                        keyframes += (*channel)->keyframes.size();
                    }
                }
            }
        }
    }
    if ((settings.bone_limit != 0 && bones > settings.bone_limit) ||
        (settings.vertex_limit != 0 && vertices > settings.vertex_limit) ||
        (settings.keyframe_limit != 0 && keyframes > settings.keyframe_limit)) {
        throw abi::Failure(omm_error_limit, omm_stage_parse,
                           "RESOURCE_LIMIT: model exceeds bone/vertex/keyframe limit");
    }
}

omm_handle publish(std::shared_ptr<abi::Runtime> runtime, std::unique_ptr<Asset> asset) {
    auto object = std::make_shared<AssetObject>();
    object->asset = std::move(asset);
    runtime->live_children.fetch_add(1);
    object->runtime = std::move(runtime);
    AssetMemoryCounter counter;
    object->asset->retained_bytes = counter.count(*object->asset);
    object->reserve_memory(object->asset->retained_bytes);
    return asset_table().insert(std::move(object));
}

} // namespace

std::shared_ptr<const Asset> retain_asset(omm_handle handle, const abi::Runtime *runtime) {
    auto object = asset_table().get(handle, omm_stage_model);
    if (object->runtime.get() != runtime) {
        throw abi::Failure(omm_error_argument, omm_stage_model, "asset belongs to a different runtime");
    }
    return std::shared_ptr<const Asset>(object, object->asset.get());
}

} // namespace omm::ysm

using omm::abi::Failure;
using omm::abi::guard;

extern "C" {

OMM_API omm_status OMM_CALL omm_ysm_probe(omm_bytes input, omm_ysm_probe_result *out_result, omm_error *error) {
    return guard(error, omm_stage_parse, [&]() -> omm_status {
        if (out_result == nullptr || out_result->struct_size < sizeof(omm_ysm_probe_result)) {
            throw Failure(omm_error_argument, omm_stage_parse, "out_result is null or too small");
        }
        omm::ysm::require_bytes(input, "input");
        const std::span<const uint8_t> bytes(input.data, static_cast<size_t>(input.size));
        const uint32_t declared = out_result->struct_size;
        std::memset(out_result, 0, sizeof(omm_ysm_probe_result));
        out_result->struct_size = declared;
        out_result->version = OMM_YSM_PROBE_RESULT_VERSION;
        out_result->format = omm::ysm::format_code(omm::ysm::probe(bytes));
        switch (omm::ysm::probe_ysgp(bytes)) {
        case omm::ysm::YsgpKind::legacy_v1:
            out_result->source_version = 1;
            break;
        case omm::ysm::YsgpKind::legacy_v2:
            out_result->source_version = 2;
            break;
        case omm::ysm::YsgpKind::v3:
            out_result->source_version = 3;
            break;
        default:
            break;
        }
        if (out_result->format == omm_ysm_asset_container) {
            out_result->source_version = 1;
        }
        out_result->parseable = (omm::ysm::kSupportedFormats & (1u << out_result->format)) != 0 ? 1u : 0u;
        // 8 bytes decide YSGP legacy and ZIP; the v3 text header needs its terminator.
        out_result->min_input_bytes = 8;
        if (out_result->format == omm_ysm_unknown) {
            return omm_error_unsupported;
        }
        return omm_ok;
    });
}

OMM_API omm_status OMM_CALL omm_ysm_parse(omm_handle runtime_handle, omm_bytes input, const omm_ysm_parse_desc *desc,
                                          omm_handle *out_asset, omm_error *error) {
    if (out_asset != nullptr) {
        *out_asset = 0;
    }
    return guard(error, omm_stage_parse, [&]() -> omm_status {
        if (out_asset == nullptr) {
            throw Failure(omm_error_argument, omm_stage_parse, "out_asset is null");
        }
        omm::ysm::require_bytes(input, "input");
        auto runtime = omm::abi::runtime_table().get(runtime_handle, omm_stage_parse);
        const omm::ysm::ParseSettings settings = omm::ysm::settings_from(*runtime, desc);
        const std::span<const uint8_t> bytes(input.data, static_cast<size_t>(input.size));
        const uint32_t format = omm::ysm::format_code(omm::ysm::probe(bytes));
        if (format == omm_ysm_unknown) {
            throw Failure(omm_error_unsupported, omm_stage_parse, "UNSUPPORTED_VERSION: unrecognized source format");
        }
        if ((settings.allowed_formats & (1u << format)) == 0) {
            throw Failure(omm_error_unsupported, omm_stage_parse, "source format is not in allowed_formats");
        }
        const bool current = format == omm_ysm_asset_container;
        if ((settings.source_profile == omm_profile_ysm_current && !current) ||
            (settings.source_profile == omm_profile_omp_compat && current)) {
            throw Failure(omm_error_unsupported, omm_stage_parse, "source format does not match requested profile");
        }
        omm::ysm::json::ScopedNestingLimit nesting(settings.nesting_limit);
        auto asset = omm::ysm::parse_container(bytes, settings.display_name, settings.limits);
        omm::ysm::enforce_model_limits(*asset, settings);
        *out_asset = omm::ysm::publish(std::move(runtime), std::move(asset));
        return omm_ok;
    });
}

OMM_API omm_status OMM_CALL omm_ysm_parse_entries(omm_handle runtime_handle, const omm_ysm_entry *entries,
                                                  uint32_t count, const omm_ysm_parse_desc *desc, omm_handle *out_asset,
                                                  omm_error *error) {
    if (out_asset != nullptr) {
        *out_asset = 0;
    }
    return guard(error, omm_stage_parse, [&]() -> omm_status {
        if (out_asset == nullptr || (entries == nullptr && count != 0)) {
            throw Failure(omm_error_argument, omm_stage_parse, "out_asset or entries is null");
        }
        auto runtime = omm::abi::runtime_table().get(runtime_handle, omm_stage_parse);
        const omm::ysm::ParseSettings settings = omm::ysm::settings_from(*runtime, desc);
        if (settings.source_profile == omm_profile_ysm_current) {
            throw Failure(omm_error_unsupported, omm_stage_parse, "current profile requires an asset container");
        }
        if ((settings.allowed_formats & (1u << omm_ysm_raw)) == 0) {
            throw Failure(omm_error_unsupported, omm_stage_parse, "raw entries are not in allowed_formats");
        }
        std::vector<omm::ysm::EntryInput> inputs;
        inputs.reserve(count);
        for (uint32_t index = 0; index < count; ++index) {
            omm::ysm::require_bytes(entries[index].path, "entry path");
            omm::ysm::require_bytes(entries[index].data, "entry data");
            std::string path(reinterpret_cast<const char *>(entries[index].path.data),
                             static_cast<size_t>(entries[index].path.size));
            if (!omm::ysm::is_valid_utf8(path)) {
                throw Failure(omm_error_argument, omm_stage_parse, "entry path is not valid UTF-8");
            }
            inputs.push_back(omm::ysm::EntryInput{
                std::move(path),
                std::span<const uint8_t>(entries[index].data.data, static_cast<size_t>(entries[index].data.size))});
        }
        omm::ysm::json::ScopedNestingLimit nesting(settings.nesting_limit);
        auto asset = omm::ysm::parse_entries(inputs, settings.display_name, settings.limits);
        omm::ysm::enforce_model_limits(*asset, settings);
        *out_asset = omm::ysm::publish(std::move(runtime), std::move(asset));
        return omm_ok;
    });
}

OMM_API omm_status OMM_CALL omm_asset_get_info(omm_handle asset_handle, omm_asset_info *out_info, omm_error *error) {
    return guard(error, omm_stage_parse, [&]() -> omm_status {
        if (out_info == nullptr || out_info->struct_size < sizeof(omm_asset_info)) {
            throw Failure(omm_error_argument, omm_stage_parse, "out_info is null or too small");
        }
        const auto object = omm::ysm::asset_table().get(asset_handle, omm_stage_parse);
        const omm::ysm::Asset &asset = *object->asset;
        std::lock_guard<std::mutex> lock(object->section_mutex);
        const uint32_t declared = out_info->struct_size;
        std::memset(out_info, 0, sizeof(omm_asset_info));
        out_info->struct_size = declared;
        out_info->version = OMM_ASSET_INFO_VERSION;
        out_info->source_format = omm::ysm::container_format(asset);
        switch (asset.source.container) {
        case omm::ysm::SourceContainer::legacy_v1:
            out_info->source_version = 1;
            break;
        case omm::ysm::SourceContainer::legacy_v2:
            out_info->source_version = 2;
            break;
        case omm::ysm::SourceContainer::v3:
            out_info->source_version = static_cast<uint32_t>(asset.source.binary_format);
            break;
        default:
            break;
        }
        out_info->asset_schema = OMM_ASSET_SCHEMA_VERSION;
        out_info->geometry_count = static_cast<uint32_t>(asset.geometries.size());
        for (const omm::ysm::Geometry &geometry : asset.geometries) {
            out_info->bone_count += static_cast<uint32_t>(geometry.bones.size());
            for (const omm::ysm::GeometryBone &bone : geometry.bones) {
                for (const omm::ysm::GeometryCube &cube : bone.cubes) {
                    out_info->quad_count += static_cast<uint32_t>(cube.quads.size());
                }
            }
        }
        out_info->animation_file_count = static_cast<uint32_t>(asset.animation_files.size());
        for (const omm::ysm::AnimationFile &file : asset.animation_files) {
            out_info->clip_count += static_cast<uint32_t>(file.clips.size());
        }
        for (const omm::ysm::ControllerFile &file : asset.controller_files) {
            out_info->controller_count += static_cast<uint32_t>(file.controllers.size());
        }
        out_info->resource_count = static_cast<uint32_t>(asset.resources.size());
        out_info->diagnostic_count = static_cast<uint32_t>(asset.diagnostics.size());
        out_info->retained_bytes = object->accounted_bytes;
        return omm_ok;
    });
}

OMM_API omm_status OMM_CALL omm_asset_read_section(omm_handle asset_handle, uint32_t section, omm_mut_bytes output,
                                                   uint64_t *out_required, omm_error *error) {
    return guard(error, omm_stage_parse, [&]() -> omm_status {
        if (out_required == nullptr || (output.data == nullptr && output.capacity != 0)) {
            throw Failure(omm_error_argument, omm_stage_parse, "out_required is null or output is invalid");
        }
        if (section < omm_section_metadata || section > omm_section_diagnostics) {
            throw Failure(omm_error_argument, omm_stage_parse, "unknown section");
        }
        const auto object = omm::ysm::asset_table().get(asset_handle, omm_stage_parse);
        std::lock_guard<std::mutex> lock(object->section_mutex);
        auto cached = object->sections.find(section);
        if (cached == object->sections.end()) {
            std::string serialized = omm::ysm::write_section(*object->asset, static_cast<omm::ysm::Section>(section));
            const uint64_t bytes = serialized.capacity() + 1;
            object->reserve_memory(bytes);
            try {
                cached = object->sections.emplace(section, std::move(serialized)).first;
            } catch (...) {
                object->runtime->retained_cpu_bytes.fetch_sub(bytes);
                object->accounted_bytes -= bytes;
                throw;
            }
        }
        const std::string &json = cached->second;
        *out_required = json.size();
        if (output.capacity == 0) {
            return omm_ok;
        }
        if (output.capacity < json.size()) {
            return omm::abi::report(error, omm_error_capacity, omm_stage_parse, "output capacity is too small");
        }
        std::memcpy(output.data, json.data(), json.size());
        return omm_ok;
    });
}

OMM_API omm_status OMM_CALL omm_asset_read_resource(omm_handle asset_handle, uint32_t resource_id, uint64_t offset,
                                                    omm_mut_bytes output, uint64_t *out_written, omm_error *error) {
    if (out_written != nullptr) {
        *out_written = 0;
    }
    return guard(error, omm_stage_parse, [&]() -> omm_status {
        if (out_written == nullptr || (output.data == nullptr && output.capacity != 0)) {
            throw Failure(omm_error_argument, omm_stage_parse, "out_written is null or output is invalid");
        }
        const auto object = omm::ysm::asset_table().get(asset_handle, omm_stage_parse);
        const auto &entries = object->asset->resources.entries();
        if (resource_id >= entries.size()) {
            throw Failure(omm_error_argument, omm_stage_parse, "resource_id is out of range");
        }
        const std::vector<uint8_t> &bytes = entries[resource_id].bytes;
        if (offset >= bytes.size()) {
            return omm_ok; // EOF
        }
        const uint64_t available = bytes.size() - offset;
        const uint64_t count = available < output.capacity ? available : output.capacity;
        std::memcpy(output.data, bytes.data() + offset, static_cast<size_t>(count));
        *out_written = count;
        return omm_ok;
    });
}

OMM_API omm_status OMM_CALL omm_asset_release(omm_handle asset_handle, omm_error *error) {
    return guard(error, omm_stage_parse, [&]() -> omm_status {
        omm::ysm::asset_table().remove(asset_handle, omm_stage_parse);
        return omm_ok;
    });
}

} // extern "C"
