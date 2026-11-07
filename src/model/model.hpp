#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include "abi/runtime.hpp"
#include "model/math.hpp"

namespace omm::model {

// Accounts retained model/instance storage in the same runtime budget as parsed assets.
class RuntimeStorage {
public:
    explicit RuntimeStorage(std::shared_ptr<abi::Runtime> runtime);
    ~RuntimeStorage();
    RuntimeStorage(const RuntimeStorage &) = delete;
    RuntimeStorage &operator=(const RuntimeStorage &) = delete;
    void reserve(uint64_t bytes);
    const std::shared_ptr<abi::Runtime> &runtime() const noexcept;

private:
    std::shared_ptr<abi::Runtime> runtime_;
    uint64_t reserved_ = 0;
};

struct Model {
    explicit Model(std::shared_ptr<abi::Runtime> runtime) : storage(std::move(runtime)) {}

    RuntimeStorage storage;
    uint32_t profile = 0;
    std::vector<omm_part_desc> parts;
    std::vector<uint32_t> parent_order;
    std::vector<omm_vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<omm_surface_desc> surfaces;
    std::vector<omm_material_desc> materials;
};

struct PartPose {
    Matrix4 local = identity_matrix();
    Matrix4 world = identity_matrix();
    Matrix3 normal{1, 0, 0, 0, 1, 0, 0, 0, 1};
    Matrix3 uv{1, 0, 0, 0, 1, 0, 0, 0, 1};
    std::array<float, 4> color{1, 1, 1, 1};
    bool visible = true;
    bool effective_visible = true;
    uint32_t material = OMM_MATERIAL_INHERIT;
};

struct Instance {
    explicit Instance(std::shared_ptr<Model> source) : model(std::move(source)), storage(model->storage.runtime()) {}

    std::shared_ptr<Model> model;
    RuntimeStorage storage;
    std::mutex mutex;
    uint64_t revision = 0;
    std::vector<PartPose> pose;
};

abi::HandleTable<Model> &model_table();
abi::HandleTable<Instance> &instance_table();
std::shared_ptr<Model> create_model(std::shared_ptr<abi::Runtime> runtime, const omm_model_desc &desc);
std::vector<PartPose> bind_pose(const Model &model);
void compose_pose(const Model &model, std::vector<PartPose> &pose);
void update_pose(Instance &instance, const omm_pose_update &update);

} // namespace omm::model
