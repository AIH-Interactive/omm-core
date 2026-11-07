#include "model/kernel_loop.hpp"

namespace omm::model {

namespace {

struct ScalarMath {
    static std::array<double, 3> position(const Matrix4 &matrix, const float *input) {
        return transform_position(matrix, {input[0], input[1], input[2]});
    }

    static std::array<double, 3> normal(const Matrix3 &matrix, const float *input) {
        return transform_normal(matrix, {input[0], input[1], input[2]});
    }
};

} // namespace

void transform_scalar(const Model &model, const std::vector<PartPose> &pose, size_t begin, size_t end,
                      omm_vertex *output) {
    detail::transform_range<ScalarMath>(model, pose, begin, end, output);
}

} // namespace omm::model
