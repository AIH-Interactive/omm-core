#include "model/kernel_loop.hpp"

#include <emmintrin.h>

namespace omm::model {

namespace {

struct Sse2Math {
    static std::array<double, 3> position(const Matrix4 &matrix, const float *input) {
        __m128d value = _mm_mul_pd(_mm_loadu_pd(matrix.data()), _mm_set1_pd(input[0]));
        value = _mm_add_pd(value, _mm_mul_pd(_mm_loadu_pd(matrix.data() + 4), _mm_set1_pd(input[1])));
        value = _mm_add_pd(value, _mm_mul_pd(_mm_loadu_pd(matrix.data() + 8), _mm_set1_pd(input[2])));
        value = _mm_add_pd(value, _mm_loadu_pd(matrix.data() + 12));
        std::array<double, 3> result;
        _mm_storeu_pd(result.data(), value);
        result[2] = ((matrix[2] * input[0] + matrix[6] * input[1]) + matrix[10] * input[2]) + matrix[14];
        return result;
    }

    static std::array<double, 3> normal(const Matrix3 &matrix, const float *input) {
        __m128d value = _mm_mul_pd(_mm_loadu_pd(matrix.data()), _mm_set1_pd(input[0]));
        value = _mm_add_pd(value, _mm_mul_pd(_mm_loadu_pd(matrix.data() + 3), _mm_set1_pd(input[1])));
        value = _mm_add_pd(value, _mm_mul_pd(_mm_loadu_pd(matrix.data() + 6), _mm_set1_pd(input[2])));
        std::array<double, 3> result;
        _mm_storeu_pd(result.data(), value);
        result[2] = (matrix[2] * input[0] + matrix[5] * input[1]) + matrix[8] * input[2];
        return result;
    }
};

} // namespace

void transform_sse2(const Model &model, const std::vector<PartPose> &pose, size_t begin, size_t end,
                    omm_vertex *output) {
    detail::transform_range<Sse2Math>(model, pose, begin, end, output);
}

} // namespace omm::model
