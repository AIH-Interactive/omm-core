#include "model/model.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace omm::model {

namespace {

[[noreturn]] void invalid(const char *message) {
    throw abi::Failure(omm_error_argument, omm_stage_model, message);
}

template <typename Value>
void require_array(const Value *data, uint32_t count) {
    if (count != 0 && data == nullptr) {
        invalid("nonempty model/pose array has null data");
    }
}

template <typename Value>
void copy_array(std::vector<Value> &output, const Value *input, uint32_t count) {
    require_array(input, count);
    if (count != 0) {
        output.assign(input, input + count);
    }
}

std::vector<uint32_t> parent_order(const std::vector<omm_part_desc> &parts) {
    std::vector<uint8_t> states(parts.size(), 0);
    std::vector<uint32_t> order;
    order.reserve(parts.size());
    std::vector<uint32_t> path;
    for (uint32_t start = 0; start < parts.size(); ++start) {
        path.clear();
        uint32_t cursor = start;
        while (cursor != OMM_PART_ROOT && states[cursor] == 0) {
            states[cursor] = 1;
            path.push_back(cursor);
            cursor = parts[cursor].parent_id;
        }
        if (cursor != OMM_PART_ROOT && states[cursor] == 1) {
            invalid("model part hierarchy contains a cycle");
        }
        for (auto part = path.rbegin(); part != path.rend(); ++part) {
            states[*part] = 2;
            order.push_back(*part);
        }
    }
    return order;
}

bool finite_vertex(const omm_vertex &vertex) {
    return std::all_of(std::begin(vertex.position), std::end(vertex.position),
                       [](float v) { return std::isfinite(v); }) &&
           std::all_of(std::begin(vertex.normal), std::end(vertex.normal), [](float v) { return std::isfinite(v); }) &&
           std::all_of(std::begin(vertex.uv), std::end(vertex.uv), [](float v) { return std::isfinite(v); });
}

} // namespace

RuntimeStorage::RuntimeStorage(std::shared_ptr<abi::Runtime> runtime) : runtime_(std::move(runtime)) {
    runtime_->live_children.fetch_add(1);
}

RuntimeStorage::~RuntimeStorage() {
    runtime_->retained_cpu_bytes.fetch_sub(reserved_);
    runtime_->live_children.fetch_sub(1);
}

void RuntimeStorage::reserve(uint64_t bytes) {
    uint64_t current = runtime_->retained_cpu_bytes.load();
    do {
        if (bytes > runtime_->cpu_memory_budget || current > runtime_->cpu_memory_budget - bytes) {
            throw abi::Failure(omm_error_limit, omm_stage_model,
                               "model/instance retained memory exceeds runtime budget");
        }
    } while (!runtime_->retained_cpu_bytes.compare_exchange_weak(current, current + bytes));
    reserved_ += bytes;
}

const std::shared_ptr<abi::Runtime> &RuntimeStorage::runtime() const noexcept {
    return runtime_;
}

abi::HandleTable<Model> &model_table() {
    static abi::HandleTable<Model> table(abi::HandleKind::model);
    return table;
}

abi::HandleTable<Instance> &instance_table() {
    static abi::HandleTable<Instance> table(abi::HandleKind::instance);
    return table;
}

std::shared_ptr<Model> create_model(std::shared_ptr<abi::Runtime> runtime, const omm_model_desc &desc) {
    if (desc.geometry_profile != omm_geometry_figura && desc.geometry_profile != omm_geometry_ysm) {
        invalid("unknown geometry profile");
    }
    // Check sizes against the retained budget before copying caller-owned arrays.
    const uint64_t minimum_bytes =
        sizeof(Model) + uint64_t(desc.part_count) * (sizeof(omm_part_desc) + sizeof(uint32_t)) +
        uint64_t(desc.vertex_count) * sizeof(omm_vertex) + uint64_t(desc.index_count) * sizeof(uint32_t) +
        uint64_t(desc.surface_count) * sizeof(omm_surface_desc) +
        uint64_t(desc.material_count) * sizeof(omm_material_desc);
    if (minimum_bytes > runtime->cpu_memory_budget) {
        throw abi::Failure(omm_error_limit, omm_stage_model, "model input exceeds runtime memory budget");
    }
    auto model = std::make_shared<Model>(std::move(runtime));
    model->profile = desc.geometry_profile;
    copy_array(model->parts, desc.parts, desc.part_count);
    copy_array(model->vertices, desc.vertices, desc.vertex_count);
    copy_array(model->indices, desc.indices, desc.index_count);
    copy_array(model->surfaces, desc.surfaces, desc.surface_count);
    copy_array(model->materials, desc.materials, desc.material_count);
    const double source_scale = desc.geometry_profile == omm_geometry_figura ? 1.0 / 16.0 : 1.0;
    for (omm_part_desc &part : model->parts) {
        if (part.parent_id != OMM_PART_ROOT && part.parent_id >= desc.part_count) {
            invalid("part parent ID is out of range");
        }
        Matrix4 matrix;
        std::copy_n(part.local_bind, 16, matrix.begin());
        if (!is_finite_affine(matrix)) {
            invalid("part bind matrix is not finite affine");
        }
        for (size_t axis = 0; axis < 3; ++axis) {
            part.local_bind[12 + axis] *= source_scale;
        }
    }
    model->parent_order = parent_order(model->parts);
    for (omm_vertex &vertex : model->vertices) {
        if (vertex.part_id >= desc.part_count || !finite_vertex(vertex)) {
            invalid("vertex contains invalid part ID or nonfinite values");
        }
        for (float &coordinate : vertex.position) {
            coordinate = static_cast<float>(coordinate * source_scale);
        }
    }
    if (model->indices.size() % 3 != 0) {
        invalid("triangle index count is not divisible by three");
    }
    for (size_t triangle = 0; triangle < model->indices.size(); triangle += 3) {
        for (size_t corner = 0; corner < 3; ++corner) {
            if (model->indices[triangle + corner] >= model->vertices.size()) {
                invalid("triangle vertex index is out of range");
            }
        }
        const uint32_t part = model->vertices[model->indices[triangle]].part_id;
        if (model->vertices[model->indices[triangle + 1]].part_id != part ||
            model->vertices[model->indices[triangle + 2]].part_id != part) {
            invalid("triangle vertices must belong to the same rigid part");
        }
    }
    uint64_t next_index = 0;
    for (const omm_surface_desc &surface : model->surfaces) {
        if (surface.first_index != next_index || surface.index_count % 3 != 0 ||
            uint64_t(surface.first_index) + surface.index_count > model->indices.size() ||
            surface.material_id >= model->materials.size() || (surface.flags & ~15u) != 0) {
            invalid("surface range, flags or material ID is invalid");
        }
        next_index += surface.index_count;
    }
    if (next_index != model->indices.size()) {
        invalid("surfaces must cover the index buffer exactly in input order");
    }
    for (const omm_material_desc &material : model->materials) {
        if (!std::isfinite(material.alpha) || material.alpha < 0 || material.alpha > 1 ||
            !std::isfinite(material.emissive) || material.emissive < 0 || material.emissive > 1) {
            invalid("material alpha/emissive is outside zero to one");
        }
    }
    // Validate bind-world normals before any public model handle is published.
    (void)bind_pose(*model);
    const uint64_t retained =
        sizeof(Model) + model->parts.capacity() * sizeof(omm_part_desc) +
        model->parent_order.capacity() * sizeof(uint32_t) + model->vertices.capacity() * sizeof(omm_vertex) +
        model->indices.capacity() * sizeof(uint32_t) + model->surfaces.capacity() * sizeof(omm_surface_desc) +
        model->materials.capacity() * sizeof(omm_material_desc);
    model->storage.reserve(retained);
    return model;
}

std::vector<PartPose> bind_pose(const Model &model) {
    std::vector<PartPose> pose(model.parts.size());
    for (size_t index = 0; index < pose.size(); ++index) {
        std::copy_n(model.parts[index].local_bind, 16, pose[index].local.begin());
    }
    compose_pose(model, pose);
    return pose;
}

void compose_pose(const Model &model, std::vector<PartPose> &pose) {
    for (uint32_t index : model.parent_order) {
        PartPose &part = pose[index];
        const uint32_t parent = model.parts[index].parent_id;
        part.world = parent == OMM_PART_ROOT ? part.local : multiply(pose[parent].world, part.local);
        part.effective_visible = part.visible && (parent == OMM_PART_ROOT || pose[parent].effective_visible);
        if (!normal_matrix(part.world, part.normal)) {
            invalid("pose has a singular or nonfinite world/normal matrix");
        }
    }
}

void update_pose(Instance &instance, const omm_pose_update &update) {
    const Model &model = *instance.model;
    if (update.revision <= instance.revision) {
        throw abi::Failure(omm_error_state, omm_stage_model, "pose revision must increase monotonically");
    }
    if ((update.mode != omm_pose_full && update.mode != omm_pose_delta) || update.part_count > model.parts.size() ||
        (update.mode == omm_pose_full && update.part_count != model.parts.size())) {
        invalid("pose mode/count is invalid");
    }
    require_array(update.part_ids, update.part_count);
    require_array(update.local_matrices, update.part_count);
    auto next = update.mode == omm_pose_full ? bind_pose(model) : instance.pose;
    std::vector<bool> seen(model.parts.size(), false);
    for (uint32_t index = 0; index < update.part_count; ++index) {
        const uint32_t id = update.part_ids[index];
        if (id >= next.size() || seen[id]) {
            invalid("pose contains duplicate or out-of-range part IDs");
        }
        seen[id] = true;
        PartPose &part = next[id];
        std::copy_n(update.local_matrices + size_t(index) * 16, 16, part.local.begin());
        if (!is_finite_affine(part.local)) {
            invalid("pose local matrix is not finite affine");
        }
        if (update.visibility != nullptr) {
            if (update.visibility[index] > 1) {
                invalid("pose visibility must be zero or one");
            }
            part.visible = update.visibility[index] != 0;
        }
        if (update.colors != nullptr) {
            std::copy_n(update.colors + size_t(index) * 4, 4, part.color.begin());
            if (!std::all_of(part.color.begin(), part.color.end(),
                             [](float v) { return std::isfinite(v) && v >= 0; })) {
                invalid("pose color must be finite and nonnegative");
            }
        }
        if (update.uv_matrices != nullptr) {
            std::copy_n(update.uv_matrices + size_t(index) * 9, 9, part.uv.begin());
            if (!std::all_of(part.uv.begin(), part.uv.end(), [](double v) { return std::isfinite(v); })) {
                invalid("pose UV matrix is not finite");
            }
        }
        if (update.materials != nullptr) {
            part.material = update.materials[index];
            if (part.material != OMM_MATERIAL_INHERIT && part.material >= model.materials.size()) {
                invalid("pose material ID is out of range");
            }
        }
    }
    compose_pose(model, next);
    instance.pose.swap(next);
    instance.revision = update.revision;
}

} // namespace omm::model
