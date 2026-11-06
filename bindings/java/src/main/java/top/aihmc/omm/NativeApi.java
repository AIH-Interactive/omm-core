package top.aihmc.omm;

import java.nio.file.Path;

/**
 * Stable JNI surface of omm-core. Methods are bound by {@code JNI_OnLoad} via RegisterNatives;
 * names and signatures of this base set never change (API document section 10).
 *
 * <p>Handles are opaque {@code long} values; 0 is invalid. Every failure raises
 * {@link OmmNativeException}.
 */
public final class NativeApi {
    public static final int CPU_AUTO = 0;
    public static final int CPU_SCALAR = 1;
    public static final int CPU_SSE2 = 2;
    public static final int CPU_AVX2 = 3;
    public static final int CPU_NEON = 4;

    private static Path loadedLibrary;

    private NativeApi() {
    }

    /**
     * Loads the native library once per JVM. Loading a different path afterwards is rejected,
     * because one JVM must hold exactly one omm-core instance.
     */
    public static synchronized void load(Path library) {
        Path absolute = library.toAbsolutePath().normalize();
        if (loadedLibrary != null) {
            if (!loadedLibrary.equals(absolute)) {
                throw new IllegalStateException("omm-core already loaded from " + loadedLibrary);
            }
            return;
        }
        System.load(absolute.toString());
        loadedLibrary = absolute;
    }

    /** Returns {abiMajor, abiMinor, assetSchema, meshSchema, cacheVersion}. */
    public static native int[] omm_get_version();

    public static native String omm_get_build_id();

    public static native long omm_runtime_create(int abiMajor, int abiMinor, int cpuMode, int workerCount,
                                                 long memoryBudget, long maxFileBytes, long maxTotalBytes);

    /** Returns {cpuKernelBitmap, activeKernel, parserFormatBitmap, workerCount}. */
    public static native int[] omm_runtime_get_caps(long runtime);

    public static native void omm_runtime_destroy(long runtime);
}
