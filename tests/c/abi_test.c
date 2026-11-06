/*
 * C11 consumer test: the public header compiles as C, layouts match the frozen
 * sizes, and every runtime error path returns the documented status.
 */
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "omm/omm.h"

_Static_assert(sizeof(omm_bytes) == 16, "omm_bytes layout");
_Static_assert(sizeof(omm_mut_bytes) == 16, "omm_mut_bytes layout");
_Static_assert(sizeof(omm_error) == 536, "omm_error layout");
_Static_assert(offsetof(omm_error, source_offset) == 16, "omm_error.source_offset");
_Static_assert(offsetof(omm_error, message) == 24, "omm_error.message");
_Static_assert(sizeof(omm_version) == 96, "omm_version layout");
_Static_assert(offsetof(omm_version, build_id) == 32, "omm_version.build_id");
_Static_assert(sizeof(omm_runtime_desc) == 48, "omm_runtime_desc layout");
_Static_assert(offsetof(omm_runtime_desc, cpu_memory_budget) == 24, "omm_runtime_desc.cpu_memory_budget");
_Static_assert(sizeof(omm_runtime_caps) == 32, "omm_runtime_caps layout");

static int failures = 0;

#define EXPECT(condition)                                                     \
    do {                                                                      \
        if (!(condition)) {                                                   \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

static omm_error new_error(void) {
    omm_error error;
    memset(&error, 0, sizeof(error));
    error.struct_size = sizeof(error);
    return error;
}

static omm_runtime_desc new_desc(void) {
    omm_runtime_desc desc;
    memset(&desc, 0, sizeof(desc));
    desc.struct_size = sizeof(desc);
    desc.version = OMM_RUNTIME_DESC_VERSION;
    desc.requested_abi_major = OMM_ABI_MAJOR;
    desc.requested_abi_minor = OMM_ABI_MINOR;
    desc.cpu_mode = omm_cpu_auto;
    return desc;
}

static void test_version(void) {
    omm_error error = new_error();
    omm_version version;
    memset(&version, 0, sizeof(version));
    version.struct_size = sizeof(version);
    EXPECT(omm_get_version(&version, &error) == omm_ok);
    EXPECT(version.abi_major == OMM_ABI_MAJOR);
    EXPECT(version.abi_minor == OMM_ABI_MINOR);
    EXPECT(version.build_id[0] != '\0');

    EXPECT(omm_get_version(NULL, &error) == omm_error_argument);
    EXPECT(error.code == omm_error_argument);
    EXPECT(error.message[0] != '\0');

    version.struct_size = 8;
    EXPECT(omm_get_version(&version, NULL) == omm_error_argument);
}

static void test_runtime_lifecycle(void) {
    omm_error error = new_error();
    omm_runtime_desc desc = new_desc();
    omm_handle runtime = 0;
    EXPECT(omm_runtime_create(&desc, &runtime, &error) == omm_ok);
    EXPECT(runtime != 0);

    omm_runtime_caps caps;
    memset(&caps, 0, sizeof(caps));
    caps.struct_size = sizeof(caps);
    EXPECT(omm_runtime_get_caps(runtime, &caps, &error) == omm_ok);
    EXPECT((caps.cpu_kernels & (1u << omm_cpu_scalar)) != 0);
    EXPECT((caps.cpu_kernels & (1u << caps.active_kernel)) != 0);
    EXPECT(caps.worker_count >= 1);

    EXPECT(omm_runtime_destroy(runtime, &error) == omm_ok);
    /* Released handle: stale generation. */
    EXPECT(omm_runtime_get_caps(runtime, &caps, &error) == omm_error_handle);
    EXPECT(omm_runtime_destroy(runtime, &error) == omm_error_handle);

    /* The recycled slot must issue a different handle value. */
    omm_handle second = 0;
    EXPECT(omm_runtime_create(&desc, &second, &error) == omm_ok);
    EXPECT(second != runtime);
    EXPECT(omm_runtime_get_caps(runtime, &caps, &error) == omm_error_handle);
    EXPECT(omm_runtime_destroy(second, &error) == omm_ok);
}

static void test_runtime_errors(void) {
    omm_error error = new_error();
    omm_handle runtime = 12345;
    omm_runtime_desc desc = new_desc();

    EXPECT(omm_runtime_create(NULL, &runtime, &error) == omm_error_argument);
    EXPECT(runtime == 0);

    desc.struct_size = 16;
    EXPECT(omm_runtime_create(&desc, &runtime, &error) == omm_error_argument);

    desc = new_desc();
    desc.requested_abi_major = OMM_ABI_MAJOR + 1u;
    EXPECT(omm_runtime_create(&desc, &runtime, &error) == omm_error_abi);
    EXPECT(error.stage == omm_stage_abi);

    desc = new_desc();
    desc.requested_abi_minor = OMM_ABI_MINOR + 1u;
    EXPECT(omm_runtime_create(&desc, &runtime, &error) == omm_error_abi);

    desc = new_desc();
    desc.cpu_mode = 99u;
    EXPECT(omm_runtime_create(&desc, &runtime, &error) == omm_error_argument);

    /* An explicit kernel that this architecture cannot provide is unsupported, not degraded. */
    desc = new_desc();
#if defined(_M_X64) || defined(__x86_64__)
    desc.cpu_mode = omm_cpu_neon;
#else
    desc.cpu_mode = omm_cpu_sse2;
#endif
    EXPECT(omm_runtime_create(&desc, &runtime, &error) == omm_error_unsupported);
    EXPECT(runtime == 0);

    desc = new_desc();
    desc.cpu_mode = omm_cpu_scalar;
    EXPECT(omm_runtime_create(&desc, &runtime, &error) == omm_ok);
    omm_runtime_caps caps;
    memset(&caps, 0, sizeof(caps));
    caps.struct_size = sizeof(caps);
    EXPECT(omm_runtime_get_caps(runtime, &caps, &error) == omm_ok);
    EXPECT(caps.active_kernel == omm_cpu_scalar);

    /* Null, garbage and wrong-kind handles. */
    EXPECT(omm_runtime_get_caps(0, &caps, &error) == omm_error_handle);
    EXPECT(omm_runtime_get_caps(0x7F00000100000000ull, &caps, &error) == omm_error_handle);
    EXPECT(omm_runtime_get_caps(runtime + 1u, &caps, &error) == omm_error_handle);
    EXPECT(omm_runtime_destroy(runtime, &error) == omm_ok);
}

int main(void) {
    test_version();
    test_runtime_lifecycle();
    test_runtime_errors();
    if (failures != 0) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("abi_test: all checks passed\n");
    return 0;
}
