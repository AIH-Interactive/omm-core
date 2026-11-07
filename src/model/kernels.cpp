#include "model/kernels.hpp"

#include <algorithm>

#include "abi/cpu_features.hpp"
#include "abi/cpu_executor.hpp"
#include "abi/parallel.hpp"

namespace omm::model {

std::vector<omm_vertex> transform_vertices(const Model &model, const std::vector<PartPose> &pose, omm_cpu_mode kernel,
                                           uint32_t threads, abi::CpuExecutor *executor) {
    const uint32_t available = abi::available_cpu_kernels();
    if (kernel == omm_cpu_auto) {
        kernel = abi::best_cpu_kernel(available);
    }
    TransformRange transform = nullptr;
    switch (kernel) {
    case omm_cpu_scalar:
        transform = transform_scalar;
        break;
#if defined(_M_X64) || defined(__x86_64__)
    case omm_cpu_sse2:
        transform = transform_sse2;
        break;
    case omm_cpu_avx2:
        transform = transform_avx2;
        break;
#elif defined(_M_ARM64) || defined(__aarch64__)
    case omm_cpu_neon:
        transform = transform_neon;
        break;
#endif
    default:
        break;
    }
    if (transform == nullptr || (available & (1u << kernel)) == 0) {
        throw abi::Failure(omm_error_unsupported, omm_stage_model, "requested vertex kernel is unavailable");
    }
    if (pose.size() != model.parts.size()) {
        throw abi::Failure(omm_error_argument, omm_stage_model, "pose size differs from model parts");
    }
    std::vector<omm_vertex> result(model.vertices.size());
    constexpr size_t block_size = 256;
    const size_t blocks = (result.size() + block_size - 1) / block_size;
    const auto work = [&](size_t block) {
        const size_t begin = block * block_size;
        transform(model, pose, begin, std::min(begin + block_size, result.size()), result.data());
    };
    if (executor != nullptr) {
        executor->run(blocks, work);
    } else {
        abi::parallel_for(blocks, threads, work);
    }
    return result;
}

} // namespace omm::model
