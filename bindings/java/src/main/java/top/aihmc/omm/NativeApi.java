package top.aihmc.omm;

import java.nio.ByteBuffer;
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

    /** workerCount: total CPU execution threads; 0 selects one per logical CPU core, 1 is serial. */
    public static native long omm_runtime_create(int abiMajor, int abiMinor, int cpuMode, int workerCount,
                                                 long memoryBudget, long maxFileBytes, long maxTotalBytes);

    /** Returns {cpuKernelBitmap, activeKernel, parserFormatBitmap, workerCount}. */
    public static native int[] omm_runtime_get_caps(long runtime);

    public static native void omm_runtime_destroy(long runtime);

    // ---- ysmparser (ABI 0.2; optional set: check omm_runtime_get_caps()[2] before use) ----

    public static final int YSM_UNKNOWN = 0;
    public static final int YSM_RAW = 1;
    public static final int YSM_ZIP = 2;
    public static final int YSM_LEGACY_V1 = 3;
    public static final int YSM_LEGACY_V2 = 4;
    public static final int YSM_LEGACY_V3 = 5;
    public static final int YSM_ASSET_CONTAINER = 6;

    public static final int SECTION_METADATA = 1;
    public static final int SECTION_ANIMATIONS = 2;
    public static final int SECTION_CONTROLLERS = 3;
    public static final int SECTION_EXPRESSIONS = 4;
    public static final int SECTION_RESOURCES = 5;
    public static final int SECTION_DIAGNOSTICS = 6;

    /** Returns {format, sourceVersion, parseable}; reads input[offset, offset + length) of a direct buffer. */
    public static native int[] omm_ysm_probe(ByteBuffer input, int offset, int length);

    /** Parses a complete container (ZIP or YSGP) from a direct buffer; returns an asset handle. */
    public static native long omm_ysm_parse(long runtime, ByteBuffer input, int offset, int length, String displayName);

    /**
     * Parses a raw model directory supplied as relative UTF-8 paths and direct buffers
     * (each buffer's position..limit; positions are not advanced).
     */
    public static native long omm_ysm_parse_entries(long runtime, String[] paths, ByteBuffer[] data, String displayName);

    /**
     * Returns {format, sourceVersion, schema, geometries, bones, quads, animationFiles, clips,
     * controllers, resources, diagnostics, retainedBytes}.
     */
    public static native long[] omm_asset_get_info(long asset);

    /** Returns the UTF-8 JSON of one section (schema_version 1). */
    public static native byte[] omm_asset_read_section(long asset, int section);

    /** Copies resource bytes into output[outputOffset, +outputLength); returns bytes written, 0 at EOF. */
    public static native int omm_asset_read_resource(long asset, int resource, long offset, ByteBuffer output,
                                                     int outputOffset, int outputLength);

    public static native void omm_asset_release(long asset);
}
