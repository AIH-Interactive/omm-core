#include "model/math.hpp"

#include <cmath>
#include <cstdio>
#include <limits>

namespace {

int failures = 0;

void expect(bool condition, const char *message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

bool close(double actual, double expected) {
    return std::abs(actual - expected) <= 1e-12;
}

void test_composition() {
    using namespace omm::model;
    auto parent = identity_matrix();
    parent[12] = 10000000.125;
    parent[13] = -2.5;
    auto child = identity_matrix();
    child[0] = 0;
    child[1] = 2;
    child[4] = -3;
    child[5] = 0;
    child[10] = -4;
    child[12] = 0.25;
    const auto world = multiply(parent, child);
    const auto point = transform_position(world, {1, 2, 3});
    expect(close(point[0], 9999994.375), "parent * child retains distant sub-block translation");
    expect(close(point[1], -0.5) && close(point[2], -12), "rotation, nonuniform scale and mirror composition");
    expect(multiply(identity_matrix(), child) == child, "left identity");
    expect(multiply(child, identity_matrix()) == child, "right identity");
}

void test_normals() {
    using namespace omm::model;
    auto position = identity_matrix();
    position[0] = -2;
    position[4] = 3;
    position[5] = 4;
    position[10] = 0.5;
    Matrix3 normal{};
    expect(normal_matrix(position, normal), "mirrored, sheared nonsingular matrix supported");
    const auto direction = transform_normal(normal, {1, 0, 0});
    expect(close(direction[0], -0.5) && close(direction[1], 0.375) && close(direction[2], 0),
           "normal uses inverse transpose, not the position matrix");
    const auto tangent = transform_position(position, {0, 1, 0});
    expect(close(direction[0] * tangent[0] + direction[1] * tangent[1], 0), "transformed normal stays perpendicular");
    const auto retained = normal;
    position[10] = 0;
    expect(!normal_matrix(position, normal) && normal == retained, "singular failure preserves prior output");
    position[10] = std::numeric_limits<double>::quiet_NaN();
    expect(!normal_matrix(position, normal) && normal == retained, "nonfinite failure preserves prior output");
    position = identity_matrix();
    position[3] = 0.5;
    expect(!is_finite_affine(position), "projective pose is rejected");
}

} // namespace

int main() {
    test_composition();
    test_normals();
    if (failures != 0) {
        return 1;
    }
    std::puts("model_math_test: passed");
    return 0;
}
