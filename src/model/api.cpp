#include "model/bake.hpp"
#include "model/model.hpp"
#include "model/kernels.hpp"
#include "model/sort.hpp"
#include "ysmparser/access.hpp"

#include <algorithm>

namespace {

void require_version(uint32_t size, uint32_t required, uint32_t version) {
    if (size < required || version < 1) {
        throw omm::abi::Failure(omm_error_argument, omm_stage_model, "model/pose struct size or version is invalid");
    }
}

} // namespace

extern "C" {

OMM_API omm_status OMM_CALL omm_model_create(omm_handle runtime, const omm_model_desc *desc, omm_handle *out_model,
                                             omm_error *error) {
    if (out_model != nullptr) {
        *out_model = 0;
    }
    return omm::abi::guard(error, omm_stage_model, [&]() -> omm_status {
        if (desc == nullptr || out_model == nullptr) {
            throw omm::abi::Failure(omm_error_argument, omm_stage_model, "model desc/output is null");
        }
        require_version(desc->struct_size, sizeof(*desc), desc->version);
        auto model = omm::model::create_model(omm::abi::runtime_table().get(runtime, omm_stage_model), *desc);
        *out_model = omm::model::model_table().insert(std::move(model));
        return omm_ok;
    });
}

OMM_API omm_status OMM_CALL omm_model_release(omm_handle model, omm_error *error) {
    return omm::abi::guard(error, omm_stage_model, [&]() -> omm_status {
        omm::model::model_table().remove(model, omm_stage_model);
        return omm_ok;
    });
}

OMM_API omm_status OMM_CALL omm_model_get_info(omm_handle model_handle, omm_model_info *out_info, omm_error *error) {
    return omm::abi::guard(error, omm_stage_model, [&]() -> omm_status {
        if (out_info == nullptr || out_info->struct_size < sizeof(*out_info)) {
            throw omm::abi::Failure(omm_error_argument, omm_stage_model, "model info output is null or too small");
        }
        const auto model = omm::model::model_table().get(model_handle, omm_stage_model);
        omm_model_info info{};
        info.struct_size = out_info->struct_size;
        info.version = OMM_MODEL_INFO_VERSION;
        info.geometry_profile = model->profile;
        info.part_count = static_cast<uint32_t>(model->parts.size());
        info.vertex_count = static_cast<uint32_t>(model->vertices.size());
        info.index_count = static_cast<uint32_t>(model->indices.size());
        info.surface_count = static_cast<uint32_t>(model->surfaces.size());
        info.material_count = static_cast<uint32_t>(model->materials.size());
        *out_info = info;
        return omm_ok;
    });
}

OMM_API omm_status OMM_CALL omm_model_create_from_asset(omm_handle runtime_handle, omm_handle asset_handle,
                                                        const omm_bake_desc *desc, omm_handle *out_model,
                                                        omm_error *error) {
    if (out_model != nullptr) {
        *out_model = 0;
    }
    return omm::abi::guard(error, omm_stage_model, [&]() -> omm_status {
        if (desc == nullptr || out_model == nullptr) {
            throw omm::abi::Failure(omm_error_argument, omm_stage_model, "bake desc/output is null");
        }
        require_version(desc->struct_size, sizeof(*desc), desc->version);
        auto runtime = omm::abi::runtime_table().get(runtime_handle, omm_stage_model);
        const auto asset = omm::ysm::retain_asset(asset_handle, runtime.get());
        auto model = omm::model::bake_asset(std::move(runtime), *asset, *desc);
        *out_model = omm::model::model_table().insert(std::move(model));
        return omm_ok;
    });
}

OMM_API omm_status OMM_CALL omm_instance_create(omm_handle model, omm_handle *out_instance, omm_error *error) {
    if (out_instance != nullptr) {
        *out_instance = 0;
    }
    return omm::abi::guard(error, omm_stage_model, [&]() -> omm_status {
        if (out_instance == nullptr) {
            throw omm::abi::Failure(omm_error_argument, omm_stage_model, "instance output is null");
        }
        auto instance = std::make_shared<omm::model::Instance>(omm::model::model_table().get(model, omm_stage_model));
        instance->pose = omm::model::bind_pose(*instance->model);
        instance->storage.reserve(sizeof(omm::model::Instance) +
                                  instance->pose.capacity() * sizeof(omm::model::PartPose));
        *out_instance = omm::model::instance_table().insert(std::move(instance));
        return omm_ok;
    });
}

OMM_API omm_status OMM_CALL omm_instance_update(omm_handle instance_handle, const omm_pose_update *update,
                                                omm_error *error) {
    return omm::abi::guard(error, omm_stage_model, [&]() -> omm_status {
        if (update == nullptr) {
            throw omm::abi::Failure(omm_error_argument, omm_stage_model, "pose update is null");
        }
        require_version(update->struct_size, sizeof(*update), update->version);
        auto instance = omm::model::instance_table().get(instance_handle, omm_stage_model);
        std::lock_guard lock(instance->mutex);
        omm::model::update_pose(*instance, *update);
        return omm_ok;
    });
}

OMM_API omm_status OMM_CALL omm_instance_query_parts(omm_handle instance_handle, const uint32_t *part_ids,
                                                     uint32_t count, omm_part_result *out_parts, omm_error *error) {
    return omm::abi::guard(error, omm_stage_model, [&]() -> omm_status {
        if (count != 0 && (part_ids == nullptr || out_parts == nullptr)) {
            throw omm::abi::Failure(omm_error_argument, omm_stage_model, "query IDs/output is null");
        }
        auto instance = omm::model::instance_table().get(instance_handle, omm_stage_model);
        std::lock_guard lock(instance->mutex);
        for (uint32_t index = 0; index < count; ++index) {
            if (part_ids[index] >= instance->pose.size()) {
                throw omm::abi::Failure(omm_error_argument, omm_stage_model, "query part ID is out of range");
            }
        }
        for (uint32_t index = 0; index < count; ++index) {
            const auto &pose = instance->pose[part_ids[index]];
            omm_part_result result{};
            result.revision = instance->revision;
            std::copy(pose.world.begin(), pose.world.end(), result.model_matrix);
            std::copy(pose.normal.begin(), pose.normal.end(), result.normal_matrix);
            std::copy(pose.uv.begin(), pose.uv.end(), result.uv_matrix);
            std::copy(pose.color.begin(), pose.color.end(), result.color);
            result.visible = pose.effective_visible ? 1u : 0u;
            result.material_id = pose.material;
            out_parts[index] = result;
        }
        return omm_ok;
    });
}

OMM_API omm_status OMM_CALL omm_instance_release(omm_handle instance, omm_error *error) {
    return omm::abi::guard(error, omm_stage_model, [&]() -> omm_status {
        omm::model::instance_table().remove(instance, omm_stage_model);
        return omm_ok;
    });
}

OMM_API omm_status OMM_CALL omm_instance_transform_cpu(omm_handle instance_handle, omm_cpu_mode kernel,
                                                      omm_vertex *output, uint32_t capacity,
                                                      uint32_t *out_required, uint64_t *out_revision, omm_error *error) {
    return omm::abi::guard(error, omm_stage_model, [&]() -> omm_status {
        if (out_required == nullptr || out_revision == nullptr || (capacity != 0 && output == nullptr)) {
            throw omm::abi::Failure(omm_error_argument, omm_stage_model, "CPU transform output is null");
        }
        const auto instance = omm::model::instance_table().get(instance_handle, omm_stage_model);
        std::vector<omm::model::PartPose> pose;
        {
            std::lock_guard lock(instance->mutex);
            *out_required = static_cast<uint32_t>(instance->model->vertices.size());
            *out_revision = instance->revision;
            if (capacity == 0) {
                return omm_ok;
            }
            if (capacity < *out_required) {
                return omm_error_capacity;
            }
            pose = instance->pose;
        }
        const auto &runtime = instance->model->storage.runtime();
        const auto vertices = omm::model::transform_vertices(*instance->model, pose, kernel, runtime->worker_count,
                                                              runtime->cpu_executor.get());
        std::copy(vertices.begin(), vertices.end(), output);
        return omm_ok;
    });
}

OMM_API omm_status OMM_CALL omm_sort_back_to_front(omm_handle runtime_handle, const omm_sort_item *items, uint32_t count,
                                                   const double camera[3], uint32_t *out_order, omm_error *error) {
    return omm::abi::guard(error, omm_stage_model, [&]() -> omm_status {
        if (camera == nullptr || (count != 0 && (items == nullptr || out_order == nullptr))) {
            throw omm::abi::Failure(omm_error_argument, omm_stage_model, "sort input/output is null");
        }
        const auto runtime = omm::abi::runtime_table().get(runtime_handle, omm_stage_model);
        const uint64_t scratch = uint64_t(count) * (sizeof(omm::model::SortItem) + sizeof(double) + sizeof(uint32_t));
        if (scratch > runtime->cpu_memory_budget) {
            throw omm::abi::Failure(omm_error_limit, omm_stage_model, "sort scratch exceeds runtime budget");
        }
        std::vector<omm::model::SortItem> input(count);
        for (uint32_t index = 0; index < count; ++index) {
            input[index] = {{items[index].center[0], items[index].center[1], items[index].center[2]}, items[index].ordinal};
        }
        const auto order = omm::model::sort_back_to_front(input, {camera[0], camera[1], camera[2]}, runtime->worker_count,
                                                           runtime->cpu_executor.get());
        std::copy(order.begin(), order.end(), out_order);
        return omm_ok;
    });
}

} // extern "C"
