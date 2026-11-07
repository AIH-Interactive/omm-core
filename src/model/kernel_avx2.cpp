#include "model/kernel_loop.hpp"

#include <immintrin.h>

namespace omm::model {

namespace {

struct Avx2Math {
    static std::array<double, 3> position(const Matrix4 &matrix, const float *input) {
        __m256d value = _mm256_mul_pd(_mm256_loadu_pd(matrix.data()), _mm256_set1_pd(input[0]));
        value = _mm256_add_pd(value, _mm256_mul_pd(_mm256_loadu_pd(matrix.data() + 4), _mm256_set1_pd(input[1])));
        value = _mm256_add_pd(value, _mm256_mul_pd(_mm256_loadu_pd(matrix.data() + 8), _mm256_set1_pd(input[2])));
        value = _mm256_add_pd(value, _mm256_loadu_pd(matrix.data() + 12));
        std::array<double, 4> result;
        _mm256_storeu_pd(result.data(), value);
        return {result[0], result[1], result[2]};
    }

    static std::array<double, 3> normal(const Matrix3 &matrix, const float *input) {
        const auto column = [&](size_t offset) {
            return _mm256_setr_pd(matrix[offset], matrix[offset + 1], matrix[offset + 2], 0);
        };
        __m256d value = _mm256_mul_pd(column(0), _mm256_set1_pd(input[0]));
        value = _mm256_add_pd(value, _mm256_mul_pd(column(3), _mm256_set1_pd(input[1])));
        value = _mm256_add_pd(value, _mm256_mul_pd(column(6), _mm256_set1_pd(input[2])));
        std::array<double, 4> result;
        _mm256_storeu_pd(result.data(), value);
        return {result[0], result[1], result[2]};
    }
};

} // namespace

void transform_avx2(const Model &model, const std::vector<PartPose> &pose, size_t begin, size_t end,
                    omm_vertex *output) {
    detail::transform_range<Avx2Math>(model, pose, begin, end, output);
}

} // namespace omm::model
