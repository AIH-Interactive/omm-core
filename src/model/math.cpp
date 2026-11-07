#include "model/math.hpp"

#include <algorithm>
#include <cmath>

namespace omm::model {

Matrix4 identity_matrix() noexcept {
    Matrix4 matrix{};
    for (size_t axis = 0; axis < 4; ++axis) {
        matrix[axis * 5] = 1.0;
    }
    return matrix;
}

Matrix4 multiply(const Matrix4 &left, const Matrix4 &right) noexcept {
    Matrix4 result{};
    for (size_t column = 0; column < 4; ++column) {
        for (size_t row = 0; row < 4; ++row) {
            double value = left[row] * right[column * 4];
            for (size_t inner = 1; inner < 4; ++inner) {
                value += left[inner * 4 + row] * right[column * 4 + inner];
            }
            result[column * 4 + row] = value;
        }
    }
    return result;
}

std::array<double, 3> transform_position(const Matrix4 &matrix, const std::array<double, 3> &position) noexcept {
    std::array<double, 3> result;
    for (size_t row = 0; row < 3; ++row) {
        result[row] = ((matrix[row] * position[0] + matrix[4 + row] * position[1]) + matrix[8 + row] * position[2]) +
                      matrix[12 + row];
    }
    return result;
}

std::array<double, 3> transform_normal(const Matrix3 &matrix, const std::array<double, 3> &normal) noexcept {
    std::array<double, 3> result;
    for (size_t row = 0; row < 3; ++row) {
        result[row] = (matrix[row] * normal[0] + matrix[3 + row] * normal[1]) + matrix[6 + row] * normal[2];
    }
    return result;
}

bool normal_matrix(const Matrix4 &position, Matrix3 &output) noexcept {
    if (!is_finite_affine(position)) {
        return false;
    }
    const double a = position[0], b = position[4], c = position[8];
    const double d = position[1], e = position[5], f = position[9];
    const double g = position[2], h = position[6], i = position[10];
    // The cofactor matrix is the inverse transpose before division by the determinant.
    Matrix3 cofactor{e * i - f * h, c * h - b * i, b * f - c * e, f * g - d * i, a * i - c * g,
                     c * d - a * f, d * h - e * g, b * g - a * h, a * e - b * d};
    const double determinant = (a * cofactor[0] + b * cofactor[3]) + c * cofactor[6];
    if (determinant == 0.0 || !std::isfinite(determinant)) {
        return false;
    }
    for (double &value : cofactor) {
        value /= determinant;
        if (!std::isfinite(value)) {
            return false;
        }
    }
    output = cofactor;
    return true;
}

bool is_finite_affine(const Matrix4 &matrix) noexcept {
    return std::all_of(matrix.begin(), matrix.end(), [](double value) { return std::isfinite(value); }) &&
           matrix[3] == 0.0 && matrix[7] == 0.0 && matrix[11] == 0.0 && matrix[15] == 1.0;
}

} // namespace omm::model
