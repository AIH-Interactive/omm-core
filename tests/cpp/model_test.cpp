#include "abi/cpu_features.hpp"
#include "model/kernels.hpp"
#include "model/model.hpp"
#include "model/sort.hpp"
#include "ysmparser/access.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

int failures = 0;

#define EXPECT(condition)                                                                                              \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);                                  \
            ++failures;                                                                                                \
        }                                                                                                              \
    } while (0)

omm_handle runtime(uint64_t budget = 0) {
    omm_runtime_desc desc{};
    desc.struct_size = sizeof(desc);
    desc.version = OMM_RUNTIME_DESC_VERSION;
    desc.requested_abi_major = OMM_ABI_MAJOR;
    desc.requested_abi_minor = OMM_ABI_MINOR;
    desc.cpu_memory_budget = budget;
    omm_handle handle = 0;
    EXPECT(omm_runtime_create(&desc, &handle, nullptr) == omm_ok);
    return handle;
}

struct Fixture {
    // Child precedes parent, exercising topology independent of input order.
    std::array<omm_part_desc, 2> parts{};
    std::array<omm_vertex, 3> vertices{};
    std::array<uint32_t, 3> indices{0, 1, 2};
    omm_surface_desc surface{0, 3, 0, omm_surface_alpha_unknown, 7};
    omm_material_desc material{0, 0, 0, 0, 1, 0, 0, 0};
    omm_model_desc desc{};

    Fixture() {
        const auto identity = omm::model::identity_matrix();
        for (auto &part : parts) {
            std::copy(identity.begin(), identity.end(), part.local_bind);
        }
        parts[0].parent_id = 1;
        parts[0].local_bind[13] = 16;
        parts[1].parent_id = OMM_PART_ROOT;
        parts[1].local_bind[12] = 32;
        for (auto &vertex : vertices) {
            vertex.normal[2] = 1;
            vertex.part_id = 0;
        }
        vertices[1].position[0] = 16;
        vertices[2].position[1] = 16;
        desc.struct_size = sizeof(desc);
        desc.version = OMM_MODEL_DESC_VERSION;
        desc.geometry_profile = omm_geometry_figura;
        desc.part_count = static_cast<uint32_t>(parts.size());
        desc.parts = parts.data();
        desc.vertex_count = static_cast<uint32_t>(vertices.size());
        desc.index_count = static_cast<uint32_t>(indices.size());
        desc.vertices = vertices.data();
        desc.indices = indices.data();
        desc.surface_count = 1;
        desc.material_count = 1;
        desc.surfaces = &surface;
        desc.materials = &material;
    }
};

omm_part_result query(omm_handle instance, uint32_t id) {
    omm_part_result result{};
    EXPECT(omm_instance_query_parts(instance, &id, 1, &result, nullptr) == omm_ok);
    return result;
}

void test_copy_pose_and_lifetime() {
    const auto root = runtime();
    Fixture fixture;
    omm_handle model = 0;
    EXPECT(omm_model_create(root, &fixture.desc, &model, nullptr) == omm_ok);
    fixture.parts[1].local_bind[12] = 999;
    fixture.vertices[1].position[0] = 999;
    {
        const auto object = omm::model::model_table().get(model, omm_stage_model);
        EXPECT(object->vertices[1].position[0] == 1);
        EXPECT(object->vertices[1].normal[2] == 1);
        EXPECT(object->indices == std::vector<uint32_t>({0, 1, 2}));
        EXPECT(object->surfaces[0].flags == omm_surface_alpha_unknown);
    }
    omm_handle first = 0, second = 0;
    EXPECT(omm_instance_create(model, &first, nullptr) == omm_ok);
    EXPECT(omm_instance_create(model, &second, nullptr) == omm_ok);
    const auto bind = query(first, 0);
    EXPECT(bind.model_matrix[12] == 2 && bind.model_matrix[13] == 1 && bind.revision == 0);
    const auto identity = omm::model::identity_matrix();
    auto parent = identity;
    parent[0] = -2;
    parent[5] = 4;
    parent[12] = 0.125;
    const uint32_t parent_id = 1;
    const uint32_t hidden = 0;
    const float color[]{0.1f, 0.2f, 0.3f, 0.4f};
    omm_pose_update update{};
    update.struct_size = sizeof(update);
    update.version = OMM_POSE_UPDATE_VERSION;
    update.revision = 10;
    update.mode = omm_pose_delta;
    update.part_count = 1;
    update.part_ids = &parent_id;
    update.local_matrices = parent.data();
    update.visibility = &hidden;
    update.colors = color;
    EXPECT(omm_instance_update(first, &update, nullptr) == omm_ok);
    const auto updated = query(first, 0);
    EXPECT(updated.revision == 10 && updated.visible == 0);
    EXPECT(updated.model_matrix[12] == 0.125 && updated.model_matrix[13] == 4);
    EXPECT(updated.normal_matrix[0] == -0.5 && updated.normal_matrix[4] == 0.25);
    EXPECT(query(second, 0).model_matrix[12] == 2);
    EXPECT(query(first, 1).color[2] == color[2]);

    // Nonmonotonic and singular updates leave the complete previous pose intact.
    EXPECT(omm_instance_update(first, &update, nullptr) == omm_error_state);
    update.revision = 11;
    parent[0] = 0;
    EXPECT(omm_instance_update(first, &update, nullptr) == omm_error_argument);
    const auto after_failure = query(first, 0);
    EXPECT(std::memcmp(&updated, &after_failure, sizeof(updated)) == 0);
    std::array<uint32_t, 2> ids{1, 1};
    std::array<double, 32> matrices{};
    std::copy(identity.begin(), identity.end(), matrices.begin());
    std::copy(identity.begin(), identity.end(), matrices.begin() + 16);
    update.mode = omm_pose_full;
    update.part_count = 2;
    update.part_ids = ids.data();
    update.local_matrices = matrices.data();
    update.visibility = nullptr;
    update.colors = nullptr;
    EXPECT(omm_instance_update(first, &update, nullptr) == omm_error_argument);
    EXPECT(query(first, 0).revision == 10);
    ids = {0, 1};
    EXPECT(omm_instance_update(first, &update, nullptr) == omm_ok);
    EXPECT(query(first, 0).visible == 1 && query(first, 1).color[2] == 1);
    ids = {0, 99};
    std::array<omm_part_result, 2> output{};
    output[0].revision = 777;
    EXPECT(omm_instance_query_parts(first, ids.data(), 2, output.data(), nullptr) == omm_error_argument);
    EXPECT(output[0].revision == 777);

    EXPECT(omm_runtime_destroy(root, nullptr) == omm_error_state);
    EXPECT(omm_model_release(model, nullptr) == omm_ok);
    EXPECT(omm_model_release(model, nullptr) == omm_error_handle);
    EXPECT(query(first, 0).revision == 11); // instance owns the model backing
    EXPECT(omm_runtime_destroy(root, nullptr) == omm_error_state);
    EXPECT(omm_instance_release(first, nullptr) == omm_ok);
    EXPECT(omm_instance_release(second, nullptr) == omm_ok);
    EXPECT(omm_instance_query_parts(first, &parent_id, 1, output.data(), nullptr) == omm_error_handle);
    EXPECT(omm::abi::runtime_table().get(root, omm_stage_model)->retained_cpu_bytes == 0);
    EXPECT(omm_runtime_destroy(root, nullptr) == omm_ok);
}

void test_rejected_models() {
    const auto root = runtime();
    Fixture fixture;
    omm_handle model = 123;
    fixture.parts[1].parent_id = 0;
    EXPECT(omm_model_create(root, &fixture.desc, &model, nullptr) == omm_error_argument && model == 0);
    fixture.parts[1].parent_id = 99;
    EXPECT(omm_model_create(root, &fixture.desc, &model, nullptr) == omm_error_argument && model == 0);
    fixture.parts[1].parent_id = OMM_PART_ROOT;
    fixture.indices[2] = 99;
    EXPECT(omm_model_create(root, &fixture.desc, &model, nullptr) == omm_error_argument && model == 0);
    fixture.indices[2] = 2;
    fixture.vertices[2].part_id = 1;
    EXPECT(omm_model_create(root, &fixture.desc, &model, nullptr) == omm_error_argument && model == 0);
    fixture.vertices[2].part_id = 0;
    fixture.surface.first_index = 1;
    EXPECT(omm_model_create(root, &fixture.desc, &model, nullptr) == omm_error_argument && model == 0);
    EXPECT(omm::abi::runtime_table().get(root, omm_stage_model)->retained_cpu_bytes == 0);
    EXPECT(omm_runtime_destroy(root, nullptr) == omm_ok);

    const auto tiny = runtime(64);
    EXPECT(omm_model_create(tiny, &fixture.desc, &model, nullptr) == omm_error_limit && model == 0);
    EXPECT(omm_runtime_destroy(tiny, nullptr) == omm_ok);
}

void test_empty_mesh() {
    const auto root = runtime();
    omm_model_desc desc{};
    desc.struct_size = sizeof(desc);
    desc.version = OMM_MODEL_DESC_VERSION;
    desc.geometry_profile = omm_geometry_ysm;
    omm_handle model = 0, instance = 0;
    EXPECT(omm_model_create(root, &desc, &model, nullptr) == omm_ok);
    EXPECT(omm_instance_create(model, &instance, nullptr) == omm_ok);
    EXPECT(omm_instance_query_parts(instance, nullptr, 0, nullptr, nullptr) == omm_ok);
    EXPECT(omm_instance_release(instance, nullptr) == omm_ok);
    EXPECT(omm_model_release(model, nullptr) == omm_ok);
    EXPECT(omm_runtime_destroy(root, nullptr) == omm_ok);
}

void test_asset_bake() {
    const auto root = runtime();
    const auto other = runtime();
    const char manifest[] = R"({"spec":2,"files":{"player":{"model":{"main":"main.json"}}}})";
    const char geometry[] = R"({"minecraft:geometry":[{
        "description":{"texture_width":16,"texture_height":16},
        "bones":[{"name":"root","pivot":[0,16,0],"rotation":[0,0,90],
          "cubes":[{"origin":[0,0,0],"size":[16,16,16],
                    "uv":{"south":{"uv":[0,0],"uv_size":[16,16]}}}]}]}]})";
    const auto bytes = [](const char *text) {
        return omm_bytes{reinterpret_cast<const uint8_t *>(text), std::strlen(text)};
    };
    const omm_ysm_entry entries[]{{bytes("ysm.json"), bytes(manifest)}, {bytes("main.json"), bytes(geometry)}};
    omm_handle asset = 0;
    EXPECT(omm_ysm_parse_entries(root, entries, 2, nullptr, &asset, nullptr) == omm_ok);
    omm_bake_desc bake{};
    bake.struct_size = sizeof(bake);
    bake.version = OMM_BAKE_DESC_VERSION;
    omm_handle model = 0;
    EXPECT(omm_model_create_from_asset(other, asset, &bake, &model, nullptr) == omm_error_argument && model == 0);
    bake.memory_budget = 1;
    EXPECT(omm_model_create_from_asset(root, asset, &bake, &model, nullptr) == omm_error_limit && model == 0);
    bake.memory_budget = 0;
    EXPECT(omm_model_create_from_asset(root, asset, &bake, &model, nullptr) == omm_ok);
    if (model != 0) {
        const auto object = omm::model::model_table().get(model, omm_stage_model);
        const auto source = omm::ysm::retain_asset(asset, object->storage.runtime().get());
        const auto &quad = source->geometries[0].bones[0].cubes[0].quads[0];
        EXPECT(object->vertices.size() == 4 && object->indices == std::vector<uint32_t>({0, 1, 2, 0, 2, 3}));
        EXPECT(object->surfaces[0].flags == omm_surface_alpha_unknown);
        EXPECT(object->vertices[0].position[0] == quad.positions[0]); // already in blocks, no second /16
        EXPECT(object->vertices[0].uv[0] == quad.uvs[0]);
    }
    omm_handle instance = 0;
    EXPECT(omm_instance_create(model, &instance, nullptr) == omm_ok);
    const auto pose = query(instance, 0);
    EXPECT(std::abs(pose.model_matrix[12] - 1) < 1e-12 && std::abs(pose.model_matrix[13] - 1) < 1e-12);
    EXPECT(std::abs(pose.model_matrix[1] - 1) < 1e-12 && std::abs(pose.model_matrix[4] + 1) < 1e-12);

    const uint8_t invisible = omm_alpha_invisible;
    bake.quad_alpha = &invisible;
    bake.quad_alpha_count = 1;
    omm_handle hidden = 0;
    EXPECT(omm_model_create_from_asset(root, asset, &bake, &hidden, nullptr) == omm_ok);
    if (hidden != 0) {
        const auto object = omm::model::model_table().get(hidden, omm_stage_model);
        EXPECT(object->indices.empty() && object->surfaces.empty() && object->parts.size() == 1);
    }
    EXPECT(omm_model_release(hidden, nullptr) == omm_ok);
    EXPECT(omm_asset_release(asset, nullptr) == omm_ok);
    EXPECT(query(instance, 0).revision == 0);
    EXPECT(omm_model_release(model, nullptr) == omm_ok);
    EXPECT(omm_instance_release(instance, nullptr) == omm_ok);
    EXPECT(omm_runtime_destroy(root, nullptr) == omm_ok);
    EXPECT(omm_runtime_destroy(other, nullptr) == omm_ok);
}

void test_cpu_kernels() {
    const auto root = runtime();
    Fixture fixture;
    // Enough vertices to execute multiple work blocks; tails are deliberately unaligned.
    std::vector<omm_vertex> vertices(4099);
    for (size_t index = 0; index < vertices.size(); ++index) {
        auto &vertex = vertices[index];
        vertex.part_id = static_cast<uint32_t>(index % 2);
        vertex.position[0] = static_cast<float>(index % 17) * 0.13f;
        vertex.position[1] = -static_cast<float>(index % 31) * 0.27f;
        vertex.position[2] = static_cast<float>(index % 7) * 0.71f;
        vertex.normal[0] = 0.25f;
        vertex.normal[1] = -0.75f;
        vertex.normal[2] = 0.5f;
        vertex.uv[0] = static_cast<float>(index % 11) * 0.1f;
        vertex.uv[1] = 0.35f;
    }
    // The first triangle has a single rigid owner; remaining vertices need not be indexed.
    vertices[0].part_id = vertices[1].part_id = vertices[2].part_id = 0;
    fixture.desc.vertices = vertices.data();
    fixture.desc.vertex_count = static_cast<uint32_t>(vertices.size());
    omm_handle handle = 0;
    EXPECT(omm_model_create(root, &fixture.desc, &handle, nullptr) == omm_ok);
    auto source = omm::model::model_table().get(handle, omm_stage_model);
    auto pose = omm::model::bind_pose(*source);
    pose[1].local[0] = -2;
    pose[1].local[4] = 0.3;
    pose[1].local[5] = 4;
    pose[1].local[10] = 0.25;
    pose[1].local[12] = 10000000.125;
    pose[0].uv[0] = 2;
    pose[0].uv[6] = 0.125;
    omm::model::compose_pose(*source, pose);
    const auto scalar = omm::model::transform_vertices(*source, pose, omm_cpu_scalar, 1);
    const uint32_t available = omm::abi::available_cpu_kernels();
    for (uint32_t kernel : {omm_cpu_scalar, omm_cpu_sse2, omm_cpu_avx2, omm_cpu_neon}) {
        if ((available & (1u << kernel)) == 0) {
            continue;
        }
        for (uint32_t threads : {1u, 8u, 16u}) {
            const auto actual = omm::model::transform_vertices(*source, pose, kernel, threads);
            EXPECT(actual.size() == scalar.size());
            EXPECT(std::memcmp(actual.data(), scalar.data(), actual.size() * sizeof(omm_vertex)) == 0);
        }
        std::printf("model_test: executed CPU kernel %u\n", kernel);
    }
    // Independent analytic checks, in addition to cross-kernel equality.
    EXPECT(scalar[0].position[0] == static_cast<float>(10000000.125));
    EXPECT(scalar[0].uv[0] == 0.125f && scalar[0].normal[2] == 2);
    EXPECT(omm_model_release(handle, nullptr) == omm_ok);
    source.reset();
    EXPECT(omm_runtime_destroy(root, nullptr) == omm_ok);
}

void test_sorting() {
    const std::vector<omm::model::SortItem> items{{{0, 0, 1}, 0}, {{0, 0, 4}, 2}, {{0, 0, -4}, 1}, {{0, 0, 4}, 2}};
    const auto expected = std::vector<uint32_t>{2, 1, 3, 0};
    for (uint32_t threads : {1u, 8u, 16u}) {
        EXPECT(omm::model::sort_back_to_front(items, {0, 0, 0}, threads) == expected);
    }
    EXPECT(omm::model::sort_back_to_front({}, {0, 0, 0}, 16).empty());
    std::vector<omm::model::SortItem> large(4099);
    for (size_t index = 0; index < large.size(); ++index) {
        large[index] = {{static_cast<double>(index % 19), static_cast<double>(index % 23), 0}, index};
    }
    const auto serial = omm::model::sort_back_to_front(large, {0.125, -0.25, 0}, 1);
    EXPECT(omm::model::sort_back_to_front(large, {0.125, -0.25, 0}, 8) == serial);
    EXPECT(omm::model::sort_back_to_front(large, {0.125, -0.25, 0}, 16) == serial);
}

} // namespace

static_assert(sizeof(omm_vertex) == 36);
static_assert(offsetof(omm_vertex, part_id) == 32);

int main() {
    test_copy_pose_and_lifetime();
    test_rejected_models();
    test_empty_mesh();
    test_asset_bake();
    test_cpu_kernels();
    test_sorting();
    if (failures != 0) {
        return 1;
    }
    std::puts("model_test: passed");
    return 0;
}
