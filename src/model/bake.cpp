#include "model/bake.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace omm::model {

namespace {

[[noreturn]] void invalid(const char *message) {
    throw abi::Failure(omm_error_argument, omm_stage_model, message);
}

Matrix4 bone_bind(const ysm::GeometryBone &bone) {
    Matrix4 rotation = identity_matrix();
    constexpr double radians = 3.14159265358979323846 / 180.0;
    // OMP YsmRenderer.applyBoneTransform: T(pivot) * Rz * Ry * Rx * T(-pivot).
    for (int axis = 2; axis >= 0; --axis) {
        const double angle = bone.rotation[axis] * radians;
        const double cosine = std::cos(angle);
        const double sine = std::sin(angle);
        Matrix4 turn = identity_matrix();
        const size_t first = static_cast<size_t>((axis + 1) % 3);
        const size_t second = static_cast<size_t>((axis + 2) % 3);
        turn[first * 4 + first] = cosine;
        turn[second * 4 + second] = cosine;
        turn[first * 4 + second] = sine;
        turn[second * 4 + first] = -sine;
        rotation = multiply(rotation, turn);
    }
    const std::array<double, 3> pivot{bone.pivot[0] / 16.0, bone.pivot[1] / 16.0, bone.pivot[2] / 16.0};
    const auto rotated = transform_position(rotation, pivot);
    for (size_t axis = 0; axis < 3; ++axis) {
        rotation[12 + axis] = pivot[axis] - rotated[axis];
    }
    return rotation;
}

} // namespace

std::shared_ptr<Model> bake_asset(std::shared_ptr<abi::Runtime> runtime, const ysm::Asset &asset,
                                  const omm_bake_desc &desc) {
    if (desc.geometry_id >= asset.geometries.size()) {
        invalid("asset geometry ID is out of range");
    }
    if (desc.geometry_profile != 0 && desc.geometry_profile != omm_geometry_ysm) {
        invalid("parsed YSM assets require the YSM coordinate profile");
    }
    const auto &geometry = asset.geometries[desc.geometry_id];
    uint64_t quad_count = 0;
    for (const auto &bone : geometry.bones) {
        for (const auto &cube : bone.cubes) {
            quad_count += cube.quads.size();
        }
    }
    if ((desc.quad_alpha == nullptr && desc.quad_alpha_count != 0) ||
        (desc.quad_alpha != nullptr && desc.quad_alpha_count != quad_count)) {
        invalid("quad alpha classification count does not match geometry");
    }
    const uint64_t budget =
        desc.memory_budget == 0 ? runtime->cpu_memory_budget : std::min(desc.memory_budget, runtime->cpu_memory_budget);
    const uint64_t estimate = sizeof(Model) + sizeof(omm_material_desc) +
                              geometry.bones.size() * (sizeof(omm_part_desc) + sizeof(uint32_t)) +
                              quad_count * (4 * sizeof(omm_vertex) + 6 * sizeof(uint32_t) + sizeof(omm_surface_desc));
    if (quad_count > std::numeric_limits<uint32_t>::max() / 6 ||
        geometry.bones.size() > std::numeric_limits<uint32_t>::max() || estimate > budget) {
        throw abi::Failure(omm_error_limit, omm_stage_model, "baked model exceeds index or memory budget");
    }
    std::vector<omm_part_desc> parts(geometry.bones.size());
    std::unordered_map<std::string, uint32_t> names;
    for (uint32_t id = 0; id < parts.size(); ++id) {
        names.emplace(geometry.bones[id].name, id);
    }
    for (size_t id = 0; id < parts.size(); ++id) {
        const auto parent = names.find(geometry.bones[id].parent);
        parts[id].parent_id = parent == names.end() ? OMM_PART_ROOT : parent->second;
        const auto bind = bone_bind(geometry.bones[id]);
        std::copy(bind.begin(), bind.end(), parts[id].local_bind);
    }
    std::vector<omm_vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<omm_surface_desc> surfaces;
    vertices.reserve(static_cast<size_t>(quad_count * 4));
    indices.reserve(static_cast<size_t>(quad_count * 6));
    surfaces.reserve(static_cast<size_t>(quad_count));
    uint64_t ordinal = 0;
    for (uint32_t id = 0; id < parts.size(); ++id) {
        for (const auto &cube : geometry.bones[id].cubes) {
            for (const auto &quad : cube.quads) {
                const uint8_t alpha = desc.quad_alpha == nullptr ? omm_alpha_unknown : desc.quad_alpha[ordinal];
                if (alpha > omm_alpha_unknown) {
                    invalid("unknown quad alpha classification");
                }
                const uint64_t original_ordinal = ordinal++;
                if (alpha == omm_alpha_invisible) {
                    continue;
                }
                const auto first_vertex = static_cast<uint32_t>(vertices.size());
                for (size_t corner = 0; corner < 4; ++corner) {
                    omm_vertex vertex{};
                    std::copy_n(quad.positions.data() + corner * 3, 3, vertex.position);
                    std::copy(quad.normal.begin(), quad.normal.end(), vertex.normal);
                    std::copy_n(quad.uvs.data() + corner * 2, 2, vertex.uv);
                    vertex.part_id = id;
                    vertices.push_back(vertex);
                }
                const uint32_t flags = alpha == omm_alpha_unknown       ? omm_surface_alpha_unknown
                                       : alpha == omm_alpha_translucent ? omm_surface_translucent
                                                                        : 0u;
                surfaces.push_back({static_cast<uint32_t>(indices.size()), 6, 0, flags, original_ordinal});
                for (uint32_t corner : {0u, 1u, 2u, 0u, 2u, 3u}) {
                    indices.push_back(first_vertex + corner);
                }
            }
        }
    }
    omm_material_desc material{};
    material.alpha = 1;
    omm_model_desc input{};
    input.struct_size = sizeof(input);
    input.version = OMM_MODEL_DESC_VERSION;
    input.geometry_profile = omm_geometry_ysm;
    input.part_count = static_cast<uint32_t>(parts.size());
    input.parts = parts.data();
    input.vertex_count = static_cast<uint32_t>(vertices.size());
    input.index_count = static_cast<uint32_t>(indices.size());
    input.vertices = vertices.data();
    input.indices = indices.data();
    input.surface_count = static_cast<uint32_t>(surfaces.size());
    input.material_count = 1;
    input.surfaces = surfaces.data();
    input.materials = &material;
    return create_model(std::move(runtime), input);
}

} // namespace omm::model
