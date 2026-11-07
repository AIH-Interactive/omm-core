#pragma once

#include <array>

namespace omm::model {

// Column-major matrices, column vectors; all CPU pose composition remains double precision.
using Matrix4 = std::array<double, 16>;
using Matrix3 = std::array<double, 9>;

Matrix4 identity_matrix() noexcept;
Matrix4 multiply(const Matrix4 &left, const Matrix4 &right) noexcept;
std::array<double, 3> transform_position(const Matrix4 &matrix, const std::array<double, 3> &position) noexcept;
std::array<double, 3> transform_normal(const Matrix3 &matrix, const std::array<double, 3> &normal) noexcept;

// Returns false for singular/non-finite transforms; never substitutes an identity normal matrix.
// On failure, output is unchanged so callers can reject an entire pose update atomically.
bool normal_matrix(const Matrix4 &position, Matrix3 &output) noexcept;
bool is_finite_affine(const Matrix4 &matrix) noexcept;

} // namespace omm::model
