#include "model/kernel_loop.hpp"

#include <arm_neon.h>

namespace omm::model {

namespace {

struct NeonMath {
    static std::array<double, 3> position(const Matrix4 &matrix, const float *input) {
        float64x2_t value = vmulq_n_f64(vld1q_f64(matrix.data()), input[0]);
        value = vaddq_f64(value, vmulq_n_f64(vld1q_f64(matrix.data() + 4), input[1]));
        value = vaddq_f64(value, vmulq_n_f64(vld1q_f64(matrix.data() + 8), input[2]));
        value = vaddq_f64(value, vld1q_f64(matrix.data() + 12));
        std::array<double, 3> result;
        vst1q_f64(result.data(), value);
        result[2] = ((matrix[2] * input[0] + matrix[6] * input[1]) + matrix[10] * input[2]) + matrix[14];
        return result;
    }

    static std::array<double, 3> normal(const Matrix3 &matrix, const float *input) {
        float64x2_t value = vmulq_n_f64(vld1q_f64(matrix.data()), input[0]);
        value = vaddq_f64(value, vmulq_n_f64(vld1q_f64(matrix.data() + 3), input[1]));
        value = vaddq_f64(value, vmulq_n_f64(vld1q_f64(matrix.data() + 6), input[2]));
        std::array<double, 3> result;
        vst1q_f64(result.data(), value);
        result[2] = (matrix[2] * input[0] + matrix[5] * input[1]) + matrix[8] * input[2];
        return result;
    }
};

} // namespace

void transform_neon(const Model &model, const std::vector<PartPose> &pose, size_t begin, size_t end,
                    omm_vertex *output) {
    detail::transform_range<NeonMath>(model, pose, begin, end, output);
}

} // namespace omm::model
