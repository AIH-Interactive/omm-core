#pragma once

#include "model/model.hpp"

namespace omm::model {

using TransformRange = void (*)(const Model &, const std::vector<PartPose> &, size_t, size_t, omm_vertex *);

void transform_scalar(const Model &, const std::vector<PartPose> &, size_t, size_t, omm_vertex *);
void transform_sse2(const Model &, const std::vector<PartPose> &, size_t, size_t, omm_vertex *);
void transform_avx2(const Model &, const std::vector<PartPose> &, size_t, size_t, omm_vertex *);
void transform_neon(const Model &, const std::vector<PartPose> &, size_t, size_t, omm_vertex *);

// Immutable model and pose are borrowed for the duration of this synchronous operation.
std::vector<omm_vertex> transform_vertices(const Model &model, const std::vector<PartPose> &pose, omm_cpu_mode kernel,
                                            uint32_t threads, abi::CpuExecutor *executor = nullptr);

} // namespace omm::model
