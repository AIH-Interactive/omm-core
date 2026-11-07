#pragma once

#include <cmath>

#include "model/kernels.hpp"

namespace omm::model::detail {

template <typename Math>
void transform_range(const Model &model, const std::vector<PartPose> &pose, size_t begin, size_t end,
                     omm_vertex *output) {
    for (size_t index = begin; index < end; ++index) {
        const auto &source = model.vertices[index];
        const auto &part = pose[source.part_id];
        const auto position = Math::position(part.world, source.position);
        const auto normal = Math::normal(part.normal, source.normal);
        omm_vertex target = source;
        for (size_t axis = 0; axis < 3; ++axis) {
            target.position[axis] = static_cast<float>(position[axis]);
            target.normal[axis] = static_cast<float>(normal[axis]);
            if (!std::isfinite(target.position[axis]) || !std::isfinite(target.normal[axis])) {
                throw abi::Failure(omm_error_argument, omm_stage_model,
                                   "transformed vertex exceeds finite float range");
            }
        }
        for (size_t axis = 0; axis < 2; ++axis) {
            target.uv[axis] = static_cast<float>((part.uv[axis] * source.uv[0] + part.uv[3 + axis] * source.uv[1]) +
                                                 part.uv[6 + axis]);
            if (!std::isfinite(target.uv[axis])) {
                throw abi::Failure(omm_error_argument, omm_stage_model, "transformed UV exceeds finite float range");
            }
        }
        output[index] = target;
    }
}

} // namespace omm::model::detail
