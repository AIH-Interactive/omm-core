package top.aihmc.omnimorph.avatar.ysm;

import com.google.gson.JsonParser;
import com.sun.management.ThreadMXBean;
import java.lang.management.ManagementFactory;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.stream.Stream;
import org.figuramc.figura.avatar.Avatar;
import top.aihmc.omm.NativeApi;
import top.aihmc.omnimorph.avatar.ysm.binary.YsmBinaryModel;
import top.aihmc.omnimorph.avatar.ysm.binary.YsmBinaryReader;
import top.aihmc.omnimorph.avatar.ysm.binary.YsmBinaryResourceMapper;
import top.aihmc.omnimorph.avatar.ysm.crypto.YsmDecodeBudget;
import top.aihmc.omnimorph.avatar.ysm.crypto.YsmLegacyDecoder;
import top.aihmc.omnimorph.avatar.ysm.crypto.YsmV3Decoder;
import top.aihmc.omnimorph.model.ysm.YsmGeometryParser;
import top.aihmc.omnimorph.model.ysm.animation.YsmAnimationParser;
import top.aihmc.omnimorph.model.ysm.controller.YsmAnimationControllerParser;

/**
 * M2 parse benchmark. Both paths start from bytes already in memory (file I/O excluded).
 *
 *   java    OMP decoders + mapping + YsmManifestReader + geometry/animation/controller parsers
 *   native  JNI omm_ysm_parse[_entries] (+ asset release)
 *   +jni    native + section transfer to Java byte[] (metadata, animations, controllers)
 *   +bind   +jni + Gson parse of the animation and controller sections (proxy for host binding)
 *
 * Usage: ParseBenchmark <omm_core library> <warmup> <iterations> <sample>...
 * Prints one TSV row per sample and path with p50 / p95 / min in milliseconds.
 */
public final class ParseBenchmark {
    private static final ThreadMXBean ALLOCATIONS = allocationBean();
    private static boolean captureRetained;
    private static long retainedBytes;

    private static ThreadMXBean allocationBean() {
        if (ManagementFactory.getThreadMXBean() instanceof ThreadMXBean bean
            && bean.isThreadAllocatedMemorySupported()) {
            if (!bean.isThreadAllocatedMemoryEnabled()) {
                bean.setThreadAllocatedMemoryEnabled(true);
            }
            return bean;
        }
        return null;
    }
    private ParseBenchmark() {}

    private record Sample(String name, boolean directory, byte[] bytes, Map<String, byte[]> files) {}

    private static Sample load(Path path) throws Exception {
        String name = path.getFileName().toString();
        if (!Files.isDirectory(path)) {
            return new Sample(name, false, Files.readAllBytes(path), null);
        }
        Map<String, byte[]> files = new LinkedHashMap<>();
        try (Stream<Path> walk = Files.walk(path)) {
            for (Path file : (Iterable<Path>) walk.filter(Files::isRegularFile)::iterator) {
                files.put(YsmPackage.normalize(path.relativize(file).toString()), Files.readAllBytes(file));
            }
        }
        return new Sample(name, true, null, files);
    }

    private static YsmDecodeBudget budget() {
        return new YsmDecodeBudget(YsmResourceLimits.DEFAULT, () -> false, 60000);
    }

    private static boolean merge(YsmResourceSource source) throws Exception {
        if (!source.exists("ysm.json")) {
            return false;
        }
        var root = YsmJson.parseObject(source.readString("ysm.json"));
        return Boolean.parseBoolean(YsmJson.string(YsmJson.obj(root, "properties"), "merge_multiline_expr", "false"));
    }

    private static int motion(YsmResourceSource source, YsmManifest manifest) throws Exception {
        boolean merge = merge(source);
        int clips = 0;
        for (String path : manifest.animationPaths()) {
            clips += YsmAnimationParser.parse(source.readString(path), true, Avatar.getMolangEngine(), merge).size();
        }
        for (String path : manifest.animationControllerPaths()) {
            clips += YsmAnimationControllerParser.parse(source.readString(path), Avatar.getMolangEngine(), null, merge)
                         .size();
        }
        return clips;
    }

    private static int raw(YsmResourceSource source) throws Exception {
        YsmManifest manifest = YsmManifestReader.read(source);
        int count = 0;
        for (String geometry : new String[] {manifest.mainModelPath(), manifest.armModelPath()}) {
            if (!geometry.isBlank()) {
                count += YsmGeometryParser.parse(source.readString(geometry)).bones.size();
            }
        }
        return count + motion(source, manifest);
    }

    private static int javaPath(Sample sample) throws Exception {
        if (sample.directory()) {
            try (YsmMemoryResourceSource source = new YsmMemoryResourceSource(sample.name(), sample.files())) {
                return raw(source);
            }
        }
        byte[] bytes = sample.bytes();
        int legacy = ByteBuffer.wrap(bytes, 4, 4).order(ByteOrder.BIG_ENDIAN).getInt();
        if (bytes[0] == 'Y' && (legacy == 1 || legacy == 2)) {
            try (YsmMemoryResourceSource source = YsmLegacyDecoder.decode(bytes, sample.name(), budget())) {
                return raw(source);
            }
        }
        YsmDecodeBudget budget = budget();
        byte[] payload = YsmV3Decoder.decode(bytes, budget);
        try (YsmBinaryModel model = YsmBinaryReader.readFilePayload(payload, budget);
            YsmBinaryResourceMapper.Mapping mapping = YsmBinaryResourceMapper.map(model, sample.name(), budget)) {
            return mapping.geometries().size() + motion(mapping.resources(), mapping.manifest());
        }
    }

    // Native inputs prepared once per sample (direct buffers), as a host would hold them.
    private static final class NativeInput {
        ByteBuffer container;
        String[] paths;
        ByteBuffer[] buffers;
    }

    private static NativeInput nativeInput(Sample sample) {
        NativeInput input = new NativeInput();
        if (!sample.directory()) {
            input.container = ByteBuffer.allocateDirect(sample.bytes().length);
            input.container.put(sample.bytes()).flip();
            return input;
        }
        input.paths = sample.files().keySet().toArray(new String[0]);
        input.buffers = new ByteBuffer[input.paths.length];
        for (int index = 0; index < input.paths.length; index++) {
            byte[] data = sample.files().get(input.paths[index]);
            input.buffers[index] = ByteBuffer.allocateDirect(data.length).put(data).flip();
        }
        return input;
    }

    private static long nativeParse(long runtime, Sample sample, NativeInput input) {
        if (input.container != null) {
            return NativeApi.omm_ysm_parse(runtime, input.container, 0, input.container.capacity(), sample.name());
        }
        return NativeApi.omm_ysm_parse_entries(runtime, input.paths, input.buffers, sample.name());
    }

    private static int nativePath(long runtime, Sample sample, NativeInput input, int stage) {
        long asset = nativeParse(runtime, sample, input);
        int size = 0;
        try {
            if (stage >= 1) {
                byte[] metadata = NativeApi.omm_asset_read_section(asset, NativeApi.SECTION_METADATA);
                byte[] animations = NativeApi.omm_asset_read_section(asset, NativeApi.SECTION_ANIMATIONS);
                byte[] controllers = NativeApi.omm_asset_read_section(asset, NativeApi.SECTION_CONTROLLERS);
                size = metadata.length + animations.length + controllers.length;
                if (stage >= 2) {
                    size +=
                        JsonParser.parseString(new String(animations, StandardCharsets.UTF_8)).getAsJsonObject().size();
                    size += JsonParser.parseString(new String(controllers, StandardCharsets.UTF_8))
                                .getAsJsonObject()
                                .size();
                }
            }
            if (captureRetained) {
                retainedBytes = NativeApi.omm_asset_get_info(asset)[11];
            }
        } finally {
            NativeApi.omm_asset_release(asset);
        }
        return size;
    }

    @FunctionalInterface
    private interface Body {
        int run() throws Exception;
    }

    private static double[] measure(int warmup, int iterations, Body body) throws Exception {
        long sink = 0;
        for (int index = 0; index < warmup; index++) {
            sink += body.run();
        }
        double[] samples = new double[iterations];
        long thread = Thread.currentThread().getId();
        long allocatedBefore = ALLOCATIONS == null ? -1 : ALLOCATIONS.getThreadAllocatedBytes(thread);
        for (int index = 0; index < iterations; index++) {
            long start = System.nanoTime();
            sink += body.run();
            samples[index] = (System.nanoTime() - start) / 1e6;
        }
        double allocated = allocatedBefore < 0
            ? -1
            : (ALLOCATIONS.getThreadAllocatedBytes(thread) - allocatedBefore) / (double) iterations;
        retainedBytes = 0;
        captureRetained = true;
        try {
            body.run();
        } finally {
            captureRetained = false;
        }
        if (sink == 42) {
            System.err.print("");
        }
        Arrays.sort(samples);
        double p50 = samples[(int) Math.floor(0.50 * (iterations - 1))];
        double p95 = samples[(int) Math.ceil(0.95 * (iterations - 1))];
        return new double[] {p50, p95, samples[0], allocated, retainedBytes};
    }

    public static void main(String[] args) throws Exception {
        NativeApi.load(Path.of(args[0]));
        int warmup = Integer.parseInt(args[1]);
        int iterations = Integer.parseInt(args[2]);
        int[] version = NativeApi.omm_get_version();
        Map<Integer, Long> runtimes = new LinkedHashMap<>();
        try {
            for (String configured : System.getenv().getOrDefault("OMM_BENCH_THREADS", "1,8,16").split(",")) {
                int workers = Integer.parseInt(configured.trim());
                if (!runtimes.containsKey(workers)) {
                    runtimes.put(workers,
                        NativeApi.omm_runtime_create(version[0], version[1], NativeApi.CPU_AUTO, workers, 0, 0, 0));
                }
            }
            System.out.println("# single model; Java parser threads=1; native=" + runtimes.keySet()
                + " warmup=" + warmup + " iterations=" + iterations);
            System.out.println(
                "sample\tpath\tthreads\tp50_ms\tp95_ms\tmin_ms\tjava_alloc_bytes_per_op\tnative_retained_bytes");
            for (int index = 3; index < args.length; index++) {
                Sample sample = load(Path.of(args[index]));
                NativeInput input = nativeInput(sample);
                List<Object[]> rows = new ArrayList<>();
                rows.add(new Object[] {"java", 1, measure(warmup, iterations, () -> javaPath(sample))});
                for (long runtime : runtimes.values()) {
                    int threads = NativeApi.omm_runtime_get_caps(runtime)[3];
                    rows.add(new Object[] {
                        "native", threads, measure(warmup, iterations, () -> nativePath(runtime, sample, input, 0))});
                    rows.add(new Object[] {"native+jni", threads,
                        measure(warmup, iterations, () -> nativePath(runtime, sample, input, 1))});
                    rows.add(new Object[] {"native+jni+bind", threads,
                        measure(warmup, iterations, () -> nativePath(runtime, sample, input, 2))});
                }
                for (Object[] row : rows) {
                    double[] values = (double[]) row[2];
                    System.out.println(String.format(Locale.ROOT, "%s\t%s\t%d\t%.2f\t%.2f\t%.2f\t%.0f\t%.0f",
                        sample.name(), row[0], row[1], values[0], values[1], values[2], values[3], values[4]));
                }
                System.out.flush();
            }
        } finally {
            for (long runtime : runtimes.values()) {
                NativeApi.omm_runtime_destroy(runtime);
            }
        }
    }
}
