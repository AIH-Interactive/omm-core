// JNI bridge for top.aihmc.omm.NativeApi.
//
// Methods are bound with RegisterNatives in JNI_OnLoad, so the only exported
// non-omm_ symbol is the JNI-mandated JNI_OnLoad. Every C status other than
// omm_ok becomes a top.aihmc.omm.OmmNativeException(code, stage, message).
#include <jni.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

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

// ---- ysmparser (ABI 0.2) ----

// Resolves a DirectByteBuffer window [offset, offset + length) without moving its position.
bool direct_window(JNIEnv *env, jobject buffer, jint offset, jint length, uint8_t *&out_data) {
    if (buffer == nullptr || offset < 0 || length < 0) {
        throw_native(env, omm_error_argument, omm_stage_jni, "buffer is null or offset/length is negative");
        return false;
    }
    void *address = env->GetDirectBufferAddress(buffer);
    const jlong capacity = env->GetDirectBufferCapacity(buffer);
    if (address == nullptr || capacity < 0) {
        throw_native(env, omm_error_argument, omm_stage_jni, "buffer is not a direct ByteBuffer");
        return false;
    }
    if (static_cast<jlong>(offset) + static_cast<jlong>(length) > capacity) {
        throw_native(env, omm_error_argument, omm_stage_jni, "offset + length exceeds the buffer capacity");
        return false;
    }
    out_data = static_cast<uint8_t *>(address) + offset;
    return true;
}

omm_ysm_parse_desc parse_desc(const std::string &display_name) {
    omm_ysm_parse_desc desc;
    std::memset(&desc, 0, sizeof(desc));
    desc.struct_size = sizeof(desc);
    desc.version = OMM_YSM_PARSE_DESC_VERSION;
    desc.display_name.data = reinterpret_cast<const uint8_t *>(display_name.data());
    desc.display_name.size = display_name.size();
    return desc;
}

// Modified UTF-8 from JNI is converted through Java's own String.getBytes(UTF_8).
bool utf8_of(JNIEnv *env, jstring text, std::string &out) {
    out.clear();
    if (text == nullptr) {
        return true;
    }
    jclass string_class = env->GetObjectClass(text);
    jmethodID get_bytes = env->GetMethodID(string_class, "getBytes", "(Ljava/lang/String;)[B");
    jstring charset = env->NewStringUTF("UTF-8");
    if (get_bytes == nullptr || charset == nullptr) {
        return false;
    }
    auto bytes = static_cast<jbyteArray>(env->CallObjectMethod(text, get_bytes, charset));
    env->DeleteLocalRef(charset);
    env->DeleteLocalRef(string_class);
    if (env->ExceptionCheck() || bytes == nullptr) {
        return false;
    }
    const jsize length = env->GetArrayLength(bytes);
    out.resize(static_cast<size_t>(length));
    env->GetByteArrayRegion(bytes, 0, length, reinterpret_cast<jbyte *>(out.data()));
    env->DeleteLocalRef(bytes);
    return true;
}

// int[] {format, source_version, parseable}; an unknown format is not an exception here.
jintArray JNICALL omm_ysm_probe_jni(JNIEnv *env, jclass, jobject input, jint offset, jint length) {
    uint8_t *data = nullptr;
    if (!direct_window(env, input, offset, length, data)) {
        return nullptr;
    }
    omm_error error = make_error();
    omm_ysm_probe_result result;
    std::memset(&result, 0, sizeof(result));
    result.struct_size = sizeof(result);
    const omm_status status = omm_ysm_probe(omm_bytes{data, static_cast<uint64_t>(length)}, &result, &error);
    if (status != omm_ok && status != omm_error_unsupported) {
        check(env, status, error);
        return nullptr;
    }
    const jint values[] = {
        static_cast<jint>(result.format),
        static_cast<jint>(result.source_version),
        static_cast<jint>(result.parseable),
    };
    return new_int_array(env, values, 3);
}

jlong JNICALL omm_ysm_parse_jni(JNIEnv *env, jclass, jlong runtime, jobject input, jint offset, jint length,
                                jstring display_name) {
    uint8_t *data = nullptr;
    std::string name;
    if (!direct_window(env, input, offset, length, data) || !utf8_of(env, display_name, name)) {
        return 0;
    }
    const omm_ysm_parse_desc desc = parse_desc(name);
    omm_error error = make_error();
    omm_handle asset = 0;
    if (!check(env, omm_ysm_parse(static_cast<omm_handle>(runtime), omm_bytes{data, static_cast<uint64_t>(length)},
                                  &desc, &asset, &error),
               error)) {
        return 0;
    }
    return static_cast<jlong>(asset);
}

// Raw directory: parallel arrays of UTF-8 relative paths and direct buffers (position..limit).
jlong JNICALL omm_ysm_parse_entries_jni(JNIEnv *env, jclass, jlong runtime, jobjectArray paths, jobjectArray buffers,
                                        jstring display_name) {
    if (paths == nullptr || buffers == nullptr || env->GetArrayLength(paths) != env->GetArrayLength(buffers)) {
        throw_native(env, omm_error_argument, omm_stage_jni, "paths and buffers must be non-null and equally long");
        return 0;
    }
    std::string name;
    if (!utf8_of(env, display_name, name)) {
        return 0;
    }
    const jsize count = env->GetArrayLength(paths);
    std::vector<std::string> path_storage(static_cast<size_t>(count));
    std::vector<omm_ysm_entry> entries(static_cast<size_t>(count));
    jclass buffer_class = env->FindClass("java/nio/Buffer");
    jmethodID position = env->GetMethodID(buffer_class, "position", "()I");
    jmethodID limit = env->GetMethodID(buffer_class, "limit", "()I");
    for (jsize index = 0; index < count; ++index) {
        auto path = static_cast<jstring>(env->GetObjectArrayElement(paths, index));
        jobject buffer = env->GetObjectArrayElement(buffers, index);
        if (path == nullptr || buffer == nullptr) {
            throw_native(env, omm_error_argument, omm_stage_jni, "null path or buffer entry");
            return 0;
        }
        const jint start = env->CallIntMethod(buffer, position);
        const jint end = env->CallIntMethod(buffer, limit);
        uint8_t *data = nullptr;
        if (!utf8_of(env, path, path_storage[static_cast<size_t>(index)]) ||
            !direct_window(env, buffer, start, end - start, data)) {
            return 0;
        }
        const std::string &stored = path_storage[static_cast<size_t>(index)];
        entries[static_cast<size_t>(index)] = omm_ysm_entry{
            omm_bytes{reinterpret_cast<const uint8_t *>(stored.data()), stored.size()},
            omm_bytes{data, static_cast<uint64_t>(end - start)}};
        env->DeleteLocalRef(path);
        env->DeleteLocalRef(buffer);
    }
    env->DeleteLocalRef(buffer_class);
    const omm_ysm_parse_desc desc = parse_desc(name);
    omm_error error = make_error();
    omm_handle asset = 0;
    if (!check(env, omm_ysm_parse_entries(static_cast<omm_handle>(runtime), entries.data(),
                                          static_cast<uint32_t>(entries.size()), &desc, &asset, &error),
               error)) {
        return 0;
    }
    return static_cast<jlong>(asset);
}

// long[] {format, version, schema, geometries, bones, quads, animation files, clips, controllers,
//         resources, diagnostics, retained bytes}
jlongArray JNICALL omm_asset_get_info_jni(JNIEnv *env, jclass, jlong asset) {
    omm_error error = make_error();
    omm_asset_info info;
    std::memset(&info, 0, sizeof(info));
    info.struct_size = sizeof(info);
    if (!check(env, omm_asset_get_info(static_cast<omm_handle>(asset), &info, &error), error)) {
        return nullptr;
    }
    const jlong values[] = {
        info.source_format,  info.source_version, info.asset_schema,     info.geometry_count,
        info.bone_count,     info.quad_count,     info.animation_file_count, info.clip_count,
        info.controller_count, info.resource_count, info.diagnostic_count,
        static_cast<jlong>(info.retained_bytes),
    };
    jlongArray array = env->NewLongArray(12);
    if (array != nullptr) {
        env->SetLongArrayRegion(array, 0, 12, values);
    }
    return array;
}

// UTF-8 JSON bytes of one section; one native → Java copy.
jbyteArray JNICALL omm_asset_read_section_jni(JNIEnv *env, jclass, jlong asset, jint section) {
    if (section < 0) {
        throw_native(env, omm_error_argument, omm_stage_jni, "section is negative");
        return nullptr;
    }
    omm_error error = make_error();
    uint64_t required = 0;
    if (!check(env, omm_asset_read_section(static_cast<omm_handle>(asset), static_cast<uint32_t>(section),
                                           omm_mut_bytes{nullptr, 0}, &required, &error),
               error)) {
        return nullptr;
    }
    if (required > 0x7FFFFFFFull) {
        throw_native(env, omm_error_limit, omm_stage_jni, "section exceeds the Java array limit");
        return nullptr;
    }
    jbyteArray array = env->NewByteArray(static_cast<jsize>(required));
    if (array == nullptr) {
        return nullptr;
    }
    // Copy through the critical region only for the memcpy; no native work runs inside it.
    void *target = env->GetPrimitiveArrayCritical(array, nullptr);
    if (target == nullptr) {
        return nullptr;
    }
    const omm_status status = omm_asset_read_section(static_cast<omm_handle>(asset), static_cast<uint32_t>(section),
                                                     omm_mut_bytes{static_cast<uint8_t *>(target), required},
                                                     &required, &error);
    env->ReleasePrimitiveArrayCritical(array, target, 0);
    if (!check(env, status, error)) {
        return nullptr;
    }
    return array;
}

// Copies resource bytes into a direct buffer window; returns bytes written (0 at EOF).
jint JNICALL omm_asset_read_resource_jni(JNIEnv *env, jclass, jlong asset, jint resource, jlong offset,
                                         jobject output, jint output_offset, jint output_length) {
    if (resource < 0 || offset < 0) {
        throw_native(env, omm_error_argument, omm_stage_jni, "resource or offset is negative");
        return 0;
    }
    uint8_t *data = nullptr;
    if (!direct_window(env, output, output_offset, output_length, data)) {
        return 0;
    }
    omm_error error = make_error();
    uint64_t written = 0;
    if (!check(env, omm_asset_read_resource(static_cast<omm_handle>(asset), static_cast<uint32_t>(resource),
                                            static_cast<uint64_t>(offset),
                                            omm_mut_bytes{data, static_cast<uint64_t>(output_length)}, &written,
                                            &error),
               error)) {
        return 0;
    }
    return static_cast<jint>(written);
}

void JNICALL omm_asset_release_jni(JNIEnv *env, jclass, jlong asset) {
    omm_error error = make_error();
    check(env, omm_asset_release(static_cast<omm_handle>(asset), &error), error);
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

// Optional sets are registered one method at a time: an older bridge class that does not
// declare a method simply does not get it, and the library still loads (API section 10).
const JNINativeMethod kParserMethods[] = {
    {const_cast<char *>("omm_ysm_probe"), const_cast<char *>("(Ljava/nio/ByteBuffer;II)[I"),
     reinterpret_cast<void *>(&omm_ysm_probe_jni)},
    {const_cast<char *>("omm_ysm_parse"), const_cast<char *>("(JLjava/nio/ByteBuffer;IILjava/lang/String;)J"),
     reinterpret_cast<void *>(&omm_ysm_parse_jni)},
    {const_cast<char *>("omm_ysm_parse_entries"),
     const_cast<char *>("(J[Ljava/lang/String;[Ljava/nio/ByteBuffer;Ljava/lang/String;)J"),
     reinterpret_cast<void *>(&omm_ysm_parse_entries_jni)},
    {const_cast<char *>("omm_asset_get_info"), const_cast<char *>("(J)[J"),
     reinterpret_cast<void *>(&omm_asset_get_info_jni)},
    {const_cast<char *>("omm_asset_read_section"), const_cast<char *>("(JI)[B"),
     reinterpret_cast<void *>(&omm_asset_read_section_jni)},
    {const_cast<char *>("omm_asset_read_resource"), const_cast<char *>("(JIJLjava/nio/ByteBuffer;II)I"),
     reinterpret_cast<void *>(&omm_asset_read_resource_jni)},
    {const_cast<char *>("omm_asset_release"), const_cast<char *>("(J)V"),
     reinterpret_cast<void *>(&omm_asset_release_jni)},
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
    if (env->RegisterNatives(api_class, kBaseMethods, count) != JNI_OK) {
        env->DeleteLocalRef(api_class);
        return JNI_ERR;
    }
    for (const JNINativeMethod &method : kParserMethods) {
        if (env->RegisterNatives(api_class, &method, 1) != JNI_OK) {
            env->ExceptionClear();  // NoSuchMethodError: this bridge predates the method
        }
    }
    env->DeleteLocalRef(api_class);
    return JNI_VERSION_1_8;
}
