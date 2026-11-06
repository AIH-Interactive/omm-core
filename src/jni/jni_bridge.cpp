// JNI bridge for top.aihmc.omm.NativeApi.
//
// Methods are bound with RegisterNatives in JNI_OnLoad, so the only exported
// non-omm_ symbol is the JNI-mandated JNI_OnLoad. Every C status other than
// omm_ok becomes a top.aihmc.omm.OmmNativeException(code, stage, message).
#include <jni.h>

#include <cstdint>
#include <cstring>

#include "omm/omm.h"

namespace {

constexpr const char *kApiClass = "top/aihmc/omm/NativeApi";
constexpr const char *kExceptionClass = "top/aihmc/omm/OmmNativeException";

jclass g_exception_class = nullptr;
jmethodID g_exception_constructor = nullptr;

omm_error make_error() noexcept {
    omm_error error;
    std::memset(&error, 0, sizeof(error));
    error.struct_size = sizeof(omm_error);
    return error;
}

void throw_native(JNIEnv *env, int32_t code, uint32_t stage, const char *message) {
    jstring text = env->NewStringUTF(message);
    if (text == nullptr) {
        return;  // OutOfMemoryError already pending.
    }
    auto exception = static_cast<jthrowable>(env->NewObject(g_exception_class, g_exception_constructor,
                                                            static_cast<jint>(code),
                                                            static_cast<jint>(stage), text));
    if (exception != nullptr) {
        env->Throw(exception);
    }
}

// Returns true when status is omm_ok; otherwise raises the bridge exception.
bool check(JNIEnv *env, omm_status status, const omm_error &error) {
    if (status == omm_ok) {
        return true;
    }
    throw_native(env, status, error.stage, error.message);
    return false;
}

// Java int carries uint32 values; negative values are out of range.
bool to_uint32(JNIEnv *env, jint value, uint32_t &out, const char *name) {
    if (value < 0) {
        throw_native(env, omm_error_argument, omm_stage_jni, name);
        return false;
    }
    out = static_cast<uint32_t>(value);
    return true;
}

bool to_uint64(JNIEnv *env, jlong value, uint64_t &out, const char *name) {
    if (value < 0) {
        throw_native(env, omm_error_argument, omm_stage_jni, name);
        return false;
    }
    out = static_cast<uint64_t>(value);
    return true;
}

jintArray new_int_array(JNIEnv *env, const jint *values, jsize count) {
    jintArray array = env->NewIntArray(count);
    if (array != nullptr) {
        env->SetIntArrayRegion(array, 0, count, values);
    }
    return array;
}

// int[] {abi_major, abi_minor, asset_schema, mesh_schema, cache_version}
jintArray JNICALL omm_get_version_jni(JNIEnv *env, jclass) {
    omm_error error = make_error();
    omm_version version;
    std::memset(&version, 0, sizeof(version));
    version.struct_size = sizeof(version);
    if (!check(env, omm_get_version(&version, &error), error)) {
        return nullptr;
    }
    const jint values[] = {
        static_cast<jint>(version.abi_major),   static_cast<jint>(version.abi_minor),
        static_cast<jint>(version.asset_schema), static_cast<jint>(version.mesh_schema),
        static_cast<jint>(version.cache_version),
    };
    return new_int_array(env, values, 5);
}

jstring JNICALL omm_get_build_id_jni(JNIEnv *env, jclass) {
    omm_error error = make_error();
    omm_version version;
    std::memset(&version, 0, sizeof(version));
    version.struct_size = sizeof(version);
    if (!check(env, omm_get_version(&version, &error), error)) {
        return nullptr;
    }
    return env->NewStringUTF(version.build_id);
}

jlong JNICALL omm_runtime_create_jni(JNIEnv *env, jclass, jint abi_major, jint abi_minor,
                                     jint cpu_mode, jint worker_count, jlong memory_budget,
                                     jlong max_file_bytes, jlong max_total_bytes) {
    omm_runtime_desc desc;
    std::memset(&desc, 0, sizeof(desc));
    desc.struct_size = sizeof(desc);
    desc.version = OMM_RUNTIME_DESC_VERSION;
    if (!to_uint32(env, abi_major, desc.requested_abi_major, "abiMajor is negative") ||
        !to_uint32(env, abi_minor, desc.requested_abi_minor, "abiMinor is negative") ||
        !to_uint32(env, cpu_mode, desc.cpu_mode, "cpuMode is negative") ||
        !to_uint32(env, worker_count, desc.worker_count, "workerCount is negative") ||
        !to_uint64(env, memory_budget, desc.cpu_memory_budget, "memoryBudget is negative") ||
        !to_uint64(env, max_file_bytes, desc.parse_max_file_bytes, "maxFileBytes is negative") ||
        !to_uint64(env, max_total_bytes, desc.parse_max_total_bytes, "maxTotalBytes is negative")) {
        return 0;
    }
    omm_error error = make_error();
    omm_handle runtime = 0;
    if (!check(env, omm_runtime_create(&desc, &runtime, &error), error)) {
        return 0;
    }
    return static_cast<jlong>(runtime);
}

// int[] {cpu_kernels, active_kernel, parser_formats, worker_count}
jintArray JNICALL omm_runtime_get_caps_jni(JNIEnv *env, jclass, jlong runtime) {
    omm_error error = make_error();
    omm_runtime_caps caps;
    std::memset(&caps, 0, sizeof(caps));
    caps.struct_size = sizeof(caps);
    if (!check(env, omm_runtime_get_caps(static_cast<omm_handle>(runtime), &caps, &error), error)) {
        return nullptr;
    }
    const jint values[] = {
        static_cast<jint>(caps.cpu_kernels),
        static_cast<jint>(caps.active_kernel),
        static_cast<jint>(caps.parser_formats),
        static_cast<jint>(caps.worker_count),
    };
    return new_int_array(env, values, 4);
}

void JNICALL omm_runtime_destroy_jni(JNIEnv *env, jclass, jlong runtime) {
    omm_error error = make_error();
    check(env, omm_runtime_destroy(static_cast<omm_handle>(runtime), &error), error);
}

// The base registration set; its names and signatures are permanently stable (API section 10).
const JNINativeMethod kBaseMethods[] = {
    {const_cast<char *>("omm_get_version"), const_cast<char *>("()[I"),
     reinterpret_cast<void *>(&omm_get_version_jni)},
    {const_cast<char *>("omm_get_build_id"), const_cast<char *>("()Ljava/lang/String;"),
     reinterpret_cast<void *>(&omm_get_build_id_jni)},
    {const_cast<char *>("omm_runtime_create"), const_cast<char *>("(IIIIJJJ)J"),
     reinterpret_cast<void *>(&omm_runtime_create_jni)},
    {const_cast<char *>("omm_runtime_get_caps"), const_cast<char *>("(J)[I"),
     reinterpret_cast<void *>(&omm_runtime_get_caps_jni)},
    {const_cast<char *>("omm_runtime_destroy"), const_cast<char *>("(J)V"),
     reinterpret_cast<void *>(&omm_runtime_destroy_jni)},
};

}  // namespace

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *) {
    JNIEnv *env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_8) != JNI_OK) {
        return JNI_ERR;
    }

    jclass exception_class = env->FindClass(kExceptionClass);
    if (exception_class == nullptr) {
        return JNI_ERR;
    }
    g_exception_class = static_cast<jclass>(env->NewGlobalRef(exception_class));
    env->DeleteLocalRef(exception_class);
    g_exception_constructor = env->GetMethodID(g_exception_class, "<init>", "(IILjava/lang/String;)V");
    if (g_exception_class == nullptr || g_exception_constructor == nullptr) {
        return JNI_ERR;
    }

    jclass api_class = env->FindClass(kApiClass);
    if (api_class == nullptr) {
        return JNI_ERR;
    }
    const auto count = static_cast<jint>(sizeof(kBaseMethods) / sizeof(kBaseMethods[0]));
    const jint registered = env->RegisterNatives(api_class, kBaseMethods, count);
    env->DeleteLocalRef(api_class);
    if (registered != JNI_OK) {
        return JNI_ERR;
    }
    return JNI_VERSION_1_8;
}
