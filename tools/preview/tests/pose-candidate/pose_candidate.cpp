#include <jni.h>
#include <emmintrin.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
using Matrix = std::array<double, 16>;

// Test-only, single-caller context. Java owns and retains the direct frame storage.
struct Context {
    std::vector<jint> parents;
    std::vector<jint> order;
    std::vector<float> inverse;
    std::vector<Matrix> world;
    float *local = nullptr;
    float *world_output = nullptr;
    float *palette_output = nullptr;
    uint8_t *visible = nullptr;
};

Matrix multiply(const Matrix &left, const float *right) {
    Matrix result;
    for (size_t column = 0; column < 4; ++column) {
        const double x = right[column * 4];
        const double y = right[column * 4 + 1];
        const double z = right[column * 4 + 2];
        const double w = right[column * 4 + 3];
        for (size_t row = 0; row < 4; ++row) {
            result[column * 4 + row] =
                ((left[row] * x + left[4 + row] * y) + left[8 + row] * z) + left[12 + row] * w;
        }
    }
    return result;
}

Matrix multiply_simd(const Matrix &left, const float *right) {
    Matrix result;
    for (size_t row = 0; row < 4; row += 2) {
        const __m128d a = _mm_loadu_pd(left.data() + row);
        const __m128d b = _mm_loadu_pd(left.data() + 4 + row);
        const __m128d c = _mm_loadu_pd(left.data() + 8 + row);
        const __m128d d = _mm_loadu_pd(left.data() + 12 + row);
        for (size_t column = 0; column < 4; ++column) {
            __m128d value = _mm_mul_pd(a, _mm_set1_pd(right[column * 4]));
            value = _mm_add_pd(value, _mm_mul_pd(b, _mm_set1_pd(right[column * 4 + 1])));
            value = _mm_add_pd(value, _mm_mul_pd(c, _mm_set1_pd(right[column * 4 + 2])));
            value = _mm_add_pd(value, _mm_mul_pd(d, _mm_set1_pd(right[column * 4 + 3])));
            _mm_storeu_pd(result.data() + column * 4 + row, value);
        }
    }
    return result;
}

void fail(JNIEnv *env, const std::exception &failure) {
    if (!env->ExceptionCheck()) {
        jclass type = env->FindClass("java/lang/IllegalStateException");
        if (type != nullptr) {
            env->ThrowNew(type, failure.what());
            env->DeleteLocalRef(type);
        }
    }
}
} // namespace

extern "C" JNIEXPORT jlong JNICALL Java_top_aihmc_omm_preview_PreviewPreparedPoseCandidate_create(
    JNIEnv *env, jclass, jintArray parents, jintArray order, jfloatArray inverse, jobject frame) {
    try {
        const auto count = static_cast<size_t>(env->GetArrayLength(parents));
        if (count == 0 || static_cast<size_t>(env->GetArrayLength(order)) != count ||
            static_cast<size_t>(env->GetArrayLength(inverse)) != count * 16) {
            throw std::invalid_argument("Invalid candidate skeleton buffers");
        }
        auto context = std::make_unique<Context>();
        context->parents.resize(count);
        context->order.resize(count);
        context->inverse.resize(count * 16);
        context->world.resize(count);
        env->GetIntArrayRegion(parents, 0, static_cast<jsize>(count), context->parents.data());
        env->GetIntArrayRegion(order, 0, static_cast<jsize>(count), context->order.data());
        env->GetFloatArrayRegion(inverse, 0, static_cast<jsize>(count * 16), context->inverse.data());
        if (env->ExceptionCheck()) {
            return 0;
        }
        std::vector<bool> seen(count, false);
        for (jint bone : context->order) {
            if (bone < 0 || static_cast<size_t>(bone) >= count || seen[bone]) {
                throw std::invalid_argument("Invalid candidate bone order");
            }
            const jint parent = context->parents[bone];
            if (parent >= 0 && (static_cast<size_t>(parent) >= count || !seen[parent])) {
                throw std::invalid_argument("Candidate parent must precede child");
            }
            seen[bone] = true;
        }
        auto *address = static_cast<float *>(env->GetDirectBufferAddress(frame));
        if (address == nullptr || env->GetDirectBufferCapacity(frame) < static_cast<jlong>(count * 193)) {
            throw std::invalid_argument("Candidate frame storage is too small");
        }
        context->local = address;
        context->world_output = address + count * 16;
        context->palette_output = address + count * 32;
        context->visible = reinterpret_cast<uint8_t *>(address + count * 48);
        return reinterpret_cast<jlong>(context.release());
    } catch (const std::exception &failure) {
        fail(env, failure);
        return 0;
    }
}

extern "C" JNIEXPORT void JNICALL Java_top_aihmc_omm_preview_PreviewPreparedPoseCandidate_compute(
    JNIEnv *env, jclass, jlong handle, jboolean simd) {
    try {
        auto &context = *reinterpret_cast<Context *>(handle);
        for (jint bone : context.order) {
            const float *local = context.local + bone * 16;
            if (!std::all_of(local, local + 16, [](float value) { return std::isfinite(value); }) ||
                local[3] != 0 || local[7] != 0 || local[11] != 0 || local[15] != 1) {
                throw std::invalid_argument("Candidate local pose must be finite affine");
            }
            const jint parent = context.parents[bone];
            auto &world = context.world[bone];
            if (parent < 0) {
                std::copy_n(local, 16, world.begin());
            } else {
                world = simd ? multiply_simd(context.world[parent], local) : multiply(context.world[parent], local);
            }
            context.visible[bone] = context.visible[bone] != 0 && (parent < 0 || context.visible[parent] != 0);
            const float *inverse = context.inverse.data() + bone * 16;
            Matrix palette = simd ? multiply_simd(world, inverse) : multiply(world, inverse);
            if (!context.visible[bone]) {
                palette = {};
                palette[15] = 1;
            }
            for (size_t value = 0; value < 16; ++value) {
                context.world_output[bone * 16 + value] = static_cast<float>(world[value]);
                context.palette_output[bone * 16 + value] = static_cast<float>(palette[value]);
            }
        }
    } catch (const std::exception &failure) {
        fail(env, failure);
    }
}

extern "C" JNIEXPORT void JNICALL Java_top_aihmc_omm_preview_PreviewPreparedPoseCandidate_destroy(
    JNIEnv *, jclass, jlong handle) {
    delete reinterpret_cast<Context *>(handle);
}
