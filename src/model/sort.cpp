#include "model/sort.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

#include "abi/error.hpp"
#include "abi/cpu_executor.hpp"
#include "abi/parallel.hpp"

namespace omm::model {

std::vector<uint32_t> sort_back_to_front(std::span<const SortItem> items, const std::array<double, 3> &camera,
                                         uint32_t threads, abi::CpuExecutor *executor) {
    if (items.size() > std::numeric_limits<uint32_t>::max() ||
        !std::all_of(camera.begin(), camera.end(), [](double value) { return std::isfinite(value); })) {
        throw abi::Failure(omm_error_argument, omm_stage_model, "sort camera or item count is invalid");
    }
    std::vector<double> distances(items.size());
    constexpr size_t block_size = 256;
    const auto work = [&](size_t block) {
        const size_t end = std::min(items.size(), (block + 1) * block_size);
        for (size_t index = block * block_size; index < end; ++index) {
            const auto &center = items[index].center;
            const double x = center[0] - camera[0];
            const double y = center[1] - camera[1];
            const double z = center[2] - camera[2];
            const double distance = (x * x + y * y) + z * z;
            if (!std::isfinite(distance)) {
                throw abi::Failure(omm_error_argument, omm_stage_model, "sort center/distance is nonfinite");
            }
            distances[index] = distance;
        }
    };
    const size_t blocks = (items.size() + block_size - 1) / block_size;
    if (executor != nullptr) {
        executor->run(blocks, work);
    } else {
        abi::parallel_for(blocks, threads, work);
    }
    std::vector<uint32_t> order(items.size());
    std::iota(order.begin(), order.end(), 0u);
    std::stable_sort(order.begin(), order.end(), [&](uint32_t left, uint32_t right) {
        if (distances[left] != distances[right]) {
            return distances[left] > distances[right];
        }
        return items[left].ordinal < items[right].ordinal;
    });
    return order;
}

} // namespace omm::model
