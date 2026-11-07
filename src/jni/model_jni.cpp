#include "jni/model_jni.hpp"

#include <cstdint>
#include <limits>

#include "abi/error.hpp"
#include "omm/omm.h"

namespace omm::jni {

namespace {

static_assert(sizeof(omm_part_desc) == 136);
static_assert(sizeof(omm_vertex) == 36);
static_assert(sizeof(omm_surface_desc) == 24);
static_assert(sizeof(omm_material_desc) == 32);
static_assert(sizeof(omm_part_result) == 304);

void throw_error(JNIEnv *env, const omm_error &error) {
    if (env->ExceptionCheck()) {
        return;
    }
    jclass type = env->FindClass("top/aihmc/omm/OmmNativeException");
    if (type == nullptr) {
        return;
    }
    const jmethodID constructor = env->GetMethodID(type, "<init>", "(IILjava/lang/String;)V");
    if (constructor == nullptr) {
        env->DeleteLocalRef(type);
        return;
    }
    jstring text = env->NewStringUTF(error.message);
    if (text == nullptr) {
        env->DeleteLocalRef(type);
        return;
    }
    auto exception = static_cast<jthrowable>(env->NewObject(type, constructor, error.code, error.stage, text));
    if (exception != nullptr) {
        env->Throw(exception);
        env->DeleteLocalRef(exception);
    }
    env->DeleteLocalRef(text);
    env->DeleteLocalRef(type);
}

template <typename Result, typename Body>
Result invoke(JNIEnv *env, Result failure, Body &&body) {
    omm_error error{};
    error.struct_size = sizeof(error);
    Result result = failure;
    const auto status = abi::guard(&error, omm_stage_jni, [&]() -> omm_status {
        result = body();
        return omm_ok;
    });
    if (status != omm_ok) {
        throw_error(env, error);
        return failure;
    }
    return result;
}

void require_ok(omm_status status, const omm_error &error) {
    if (status != omm_ok) {
        throw abi::Failure(status, error.stage, error.message, error.source_offset);
    }
}

[[noreturn]] void invalid(const char *message) {
    throw abi::Failure(omm_error_argument, omm_stage_jni, message);
}

template <typename Value>
Value *records(JNIEnv *env, jobject buffer, jint count, bool optional = false, bool writable = false) {
    if (count < 0) {
        invalid("model buffer record count is negative");
    }
    if (count == 0 || (optional && buffer == nullptr)) {
        return nullptr;
    }
    if (buffer == nullptr) {
        invalid("required model buffer is null");
    }
    const jlong bytes = static_cast<jlong>(count) * sizeof(Value);
    const jlong capacity = env->GetDirectBufferCapacity(buffer);
    void *address = env->GetDirectBufferAddress(buffer);
    if (address == nullptr || capacity < bytes || reinterpret_cast<uintptr_t>(address) % alignof(Value) != 0) {
        invalid("model buffer must be direct, aligned and large enough for every record");
    }
    if (writable) {
        jclass type = env->GetObjectClass(buffer);
        if (type == nullptr) {
            throw std::bad_alloc();
        }
        const jmethodID is_read_only = env->GetMethodID(type, "isReadOnly", "()Z");
        if (is_read_only == nullptr) {
            env->DeleteLocalRef(type);
            invalid("output object is not a ByteBuffer");
        }
        const bool read_only = env->CallBooleanMethod(buffer, is_read_only) != JNI_FALSE;
        env->DeleteLocalRef(type);
        if (read_only) {
            invalid("model output buffer is read-only");
        }
    }
    return static_cast<Value *>(address);
}

jint multiplied_count(jint count, jint stride) {
    if (count < 0 || count > std::numeric_limits<jint>::max() / stride) {
        invalid("model component count overflows the Java buffer limit");
    }
    return count * stride;
}

jlong JNICALL create(JNIEnv *env, jclass, jlong runtime, jint profile,
                       jint part_count, jobject parts, jint vertex_count, jobject vertices,
                       jint index_count, jobject indices, jint surface_count, jobject surfaces,
                       jint material_count, jobject materials) {
    return invoke<jlong>(env, 0, [&]() -> jlong {
        omm_model_desc desc{};
        desc.struct_size = sizeof(desc);
        desc.version = OMM_MODEL_DESC_VERSION;
        desc.geometry_profile = static_cast<uint32_t>(profile);
        desc.parts = records<omm_part_desc>(env, parts, part_count);
        desc.vertices = records<omm_vertex>(env, vertices, vertex_count);
        desc.indices = records<uint32_t>(env, indices, index_count);
        desc.surfaces = records<omm_surface_desc>(env, surfaces, surface_count);
        desc.materials = records<omm_material_desc>(env, materials, material_count);
        desc.part_count = static_cast<uint32_t>(part_count);
        desc.vertex_count = static_cast<uint32_t>(vertex_count);
        desc.index_count = static_cast<uint32_t>(index_count);
        desc.surface_count = static_cast<uint32_t>(surface_count);
        desc.material_count = static_cast<uint32_t>(material_count);
        omm_error error{};
        error.struct_size = sizeof(error);
        omm_handle model = 0;
        require_ok(omm_model_create(static_cast<omm_handle>(runtime), &desc, &model, &error), error);
        return static_cast<jlong>(model);
    });
}

jlong JNICALL bake(JNIEnv *env, jclass, jlong runtime, jlong asset, jint geometry,
                     jobject alpha, jint alpha_count, jlong budget) {
    return invoke<jlong>(env, 0, [&]() -> jlong {
        if (geometry < 0 || budget < 0) {
            invalid("geometry ID or bake budget is negative");
        }
        omm_bake_desc desc{};
        desc.struct_size = sizeof(desc);
        desc.version = OMM_BAKE_DESC_VERSION;
        desc.geometry_id = static_cast<uint32_t>(geometry);
        desc.quad_alpha = records<uint8_t>(env, alpha, alpha_count);
        desc.quad_alpha_count = static_cast<uint32_t>(alpha_count);
        desc.memory_budget = static_cast<uint64_t>(budget);
        omm_error error{};
        error.struct_size = sizeof(error);
        omm_handle model = 0;
        require_ok(omm_model_create_from_asset(static_cast<omm_handle>(runtime), static_cast<omm_handle>(asset),
                                                &desc, &model, &error), error);
        return static_cast<jlong>(model);
    });
}

jintArray JNICALL info(JNIEnv *env, jclass, jlong model) {
    return invoke<jintArray>(env, nullptr, [&]() -> jintArray {
        omm_model_info value{};
        value.struct_size = sizeof(value);
        omm_error error{};
        error.struct_size = sizeof(error);
        require_ok(omm_model_get_info(static_cast<omm_handle>(model), &value, &error), error);
        const jint values[]{static_cast<jint>(value.geometry_profile), static_cast<jint>(value.part_count),
                            static_cast<jint>(value.vertex_count), static_cast<jint>(value.index_count),
                            static_cast<jint>(value.surface_count), static_cast<jint>(value.material_count)};
        jintArray result = env->NewIntArray(6);
        if (result != nullptr) {
            env->SetIntArrayRegion(result, 0, 6, values);
        }
        return result;
    });
}

jlong JNICALL instance_create(JNIEnv *env, jclass, jlong model) {
    return invoke<jlong>(env, 0, [&]() -> jlong {
        omm_error error{};
        error.struct_size = sizeof(error);
        omm_handle instance = 0;
        require_ok(omm_instance_create(static_cast<omm_handle>(model), &instance, &error), error);
        return static_cast<jlong>(instance);
    });
}

void JNICALL update(JNIEnv *env, jclass, jlong instance, jlong revision, jint mode, jint count,
                       jobject ids, jobject matrices, jobject visibility, jobject colors, jobject uv, jobject materials) {
    invoke<int>(env, 0, [&]() {
        if (revision < 0) {
            invalid("pose revision is negative");
        }
        omm_pose_update desc{};
        desc.struct_size = sizeof(desc);
        desc.version = OMM_POSE_UPDATE_VERSION;
        desc.revision = static_cast<uint64_t>(revision);
        desc.mode = static_cast<uint32_t>(mode);
        desc.part_ids = records<uint32_t>(env, ids, count);
        desc.local_matrices = records<double>(env, matrices, multiplied_count(count, 16));
        desc.visibility = records<uint32_t>(env, visibility, count, true);
        desc.colors = records<float>(env, colors, multiplied_count(count, 4), true);
        desc.uv_matrices = records<double>(env, uv, multiplied_count(count, 9), true);
        desc.materials = records<uint32_t>(env, materials, count, true);
        desc.part_count = static_cast<uint32_t>(count);
        omm_error error{};
        error.struct_size = sizeof(error);
        require_ok(omm_instance_update(static_cast<omm_handle>(instance), &desc, &error), error);
        return 0;
    });
}

void JNICALL query(JNIEnv *env, jclass, jlong instance, jint count, jobject ids, jobject output) {
    invoke<int>(env, 0, [&]() {
        const auto *part_ids = records<uint32_t>(env, ids, count);
        auto *result = records<omm_part_result>(env, output, count, false, true);
        omm_error error{};
        error.struct_size = sizeof(error);
        require_ok(omm_instance_query_parts(static_cast<omm_handle>(instance), part_ids,
                                              static_cast<uint32_t>(count), result, &error), error);
        return 0;
    });
}

jlong JNICALL transform(JNIEnv *env, jclass, jlong instance, jint kernel, jint capacity, jobject output) {
    return invoke<jlong>(env, 0, [&]() -> jlong {
        auto *vertices = records<omm_vertex>(env, output, capacity, false, true);
        omm_error error{};
        error.struct_size = sizeof(error);
        uint32_t required = 0;
        uint64_t revision = 0;
        require_ok(omm_instance_transform_cpu(static_cast<omm_handle>(instance), static_cast<uint32_t>(kernel),
                                                 vertices, static_cast<uint32_t>(capacity), &required, &revision,
                                                 &error), error);
        return static_cast<jlong>(revision);
    });
}

void JNICALL model_release(JNIEnv *env, jclass, jlong handle) {
    invoke<int>(env, 0, [&]() {
        omm_error error{};
        error.struct_size = sizeof(error);
        require_ok(omm_model_release(static_cast<omm_handle>(handle), &error), error);
        return 0;
    });
}

void JNICALL instance_release(JNIEnv *env, jclass, jlong handle) {
    invoke<int>(env, 0, [&]() {
        omm_error error{};
        error.struct_size = sizeof(error);
        require_ok(omm_instance_release(static_cast<omm_handle>(handle), &error), error);
        return 0;
    });
}

void JNICALL sort(JNIEnv *env, jclass, jlong runtime, jint count, jobject items, jobject camera, jobject output) {
    invoke<int>(env, 0, [&]() {
        const auto *input = records<omm_sort_item>(env, items, count);
        const auto *view = records<double>(env, camera, 3);
        auto *order = records<uint32_t>(env, output, count, false, true);
        omm_error error{};
        error.struct_size = sizeof(error);
        require_ok(omm_sort_back_to_front(static_cast<omm_handle>(runtime), input, static_cast<uint32_t>(count),
                                             view, order, &error), error);
        return 0;
    });
}

const JNINativeMethod methods[]{
    {const_cast<char *>("omm_model_create"),
     const_cast<char *>("(JIILjava/nio/ByteBuffer;ILjava/nio/ByteBuffer;ILjava/nio/ByteBuffer;ILjava/nio/ByteBuffer;ILjava/nio/ByteBuffer;)J"),
     reinterpret_cast<void *>(&create)},
    {const_cast<char *>("omm_model_create_from_asset"), const_cast<char *>("(JJILjava/nio/ByteBuffer;IJ)J"),
     reinterpret_cast<void *>(&bake)},
    {const_cast<char *>("omm_model_get_info"), const_cast<char *>("(J)[I"), reinterpret_cast<void *>(&info)},
    {const_cast<char *>("omm_model_release"), const_cast<char *>("(J)V"), reinterpret_cast<void *>(&model_release)},
    {const_cast<char *>("omm_instance_create"), const_cast<char *>("(J)J"), reinterpret_cast<void *>(&instance_create)},
    {const_cast<char *>("omm_instance_update"),
     const_cast<char *>("(JJIILjava/nio/ByteBuffer;Ljava/nio/ByteBuffer;Ljava/nio/ByteBuffer;Ljava/nio/ByteBuffer;Ljava/nio/ByteBuffer;Ljava/nio/ByteBuffer;)V"),
     reinterpret_cast<void *>(&update)},
    {const_cast<char *>("omm_instance_query_parts"),
     const_cast<char *>("(JILjava/nio/ByteBuffer;Ljava/nio/ByteBuffer;)V"), reinterpret_cast<void *>(&query)},
    {const_cast<char *>("omm_instance_transform_cpu"), const_cast<char *>("(JIILjava/nio/ByteBuffer;)J"),
     reinterpret_cast<void *>(&transform)},
    {const_cast<char *>("omm_instance_release"), const_cast<char *>("(J)V"), reinterpret_cast<void *>(&instance_release)},
    {const_cast<char *>("omm_sort_back_to_front"),
     const_cast<char *>("(JILjava/nio/ByteBuffer;Ljava/nio/ByteBuffer;Ljava/nio/ByteBuffer;)V"),
     reinterpret_cast<void *>(&sort)},
};

} // namespace

void register_model_methods(JNIEnv *env, jclass api_class) {
    for (const auto &method : methods) {
        if (env->RegisterNatives(api_class, &method, 1) != JNI_OK) {
            env->ExceptionClear();
        }
    }
}

} // namespace omm::jni
