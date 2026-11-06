import java.nio.file.Path;

import top.aihmc.omm.NativeApi;
import top.aihmc.omm.OmmNativeException;

/**
 * Loads the omm-core library through the bridge and exercises the base JNI set,
 * including error-to-exception conversion. Usage: JniLoadTest <library-path>.
 */
public final class JniLoadTest {
    private static int failures;

    private JniLoadTest() {
    }

    private static void expect(boolean condition, String what) {
        if (!condition) {
            System.err.println("FAIL: " + what);
            failures++;
        }
    }

    private static void expectStatus(int code, Runnable action, String what) {
        try {
            action.run();
            expect(false, what + " did not throw");
        } catch (OmmNativeException exception) {
            expect(exception.code() == code, what + " code " + exception.code() + " != " + code);
        }
    }

    public static void main(String[] args) {
        Path library = Path.of(args[0]);
        NativeApi.load(library);
        NativeApi.load(library);  // Same path is idempotent.

        int[] version = NativeApi.omm_get_version();
        expect(version.length == 5, "version array length");
        String buildId = NativeApi.omm_get_build_id();
        expect(buildId != null && !buildId.isEmpty(), "build id present");

        long runtime = NativeApi.omm_runtime_create(version[0], version[1], NativeApi.CPU_AUTO, 0, 0, 0, 0);
        expect(runtime != 0, "runtime handle non-zero");
        int[] caps = NativeApi.omm_runtime_get_caps(runtime);
        expect((caps[0] & (1 << NativeApi.CPU_SCALAR)) != 0, "scalar kernel available");
        expect((caps[0] & (1 << caps[1])) != 0, "active kernel is available");
        NativeApi.omm_runtime_destroy(runtime);

        expectStatus(OmmNativeException.HANDLE, () -> NativeApi.omm_runtime_get_caps(runtime), "stale handle");
        expectStatus(OmmNativeException.HANDLE, () -> NativeApi.omm_runtime_destroy(0), "null handle");
        expectStatus(OmmNativeException.ABI,
                () -> NativeApi.omm_runtime_create(version[0] + 1, 0, 0, 0, 0, 0, 0), "ABI major mismatch");
        expectStatus(OmmNativeException.ARGUMENT,
                () -> NativeApi.omm_runtime_create(version[0], 0, -1, 0, 0, 0, 0), "negative uint32");
        expectStatus(OmmNativeException.ARGUMENT,
                () -> NativeApi.omm_runtime_create(version[0], 0, 99, 0, 0, 0, 0), "unknown cpu mode");

        if (failures != 0) {
            System.err.println(failures + " failure(s) on Java " + Runtime.version());
            System.exit(1);
        }
        System.out.println("JniLoadTest: passed on Java " + Runtime.version() + ", ABI " + version[0] + "."
                + version[1] + ", build " + buildId + ", kernels 0x" + Integer.toHexString(caps[0])
                + " active " + caps[1]);
    }
}
