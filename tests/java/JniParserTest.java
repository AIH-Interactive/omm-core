import java.io.ByteArrayOutputStream;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.nio.file.Path;
import java.nio.file.Files;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

import top.aihmc.omm.NativeApi;
import top.aihmc.omm.OmmNativeException;

/**
 * Exercises the ysmparser JNI surface on a synthetic model: raw entries, a nested ZIP from a
 * direct buffer, info/section/resource reads, error conversion and handle release.
 * Usage: JniParserTest <library-path>
 */
public final class JniParserTest {
    private static int failures;

    private JniParserTest() {
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

    private static ByteBuffer direct(byte[] bytes) {
        ByteBuffer buffer = ByteBuffer.allocateDirect(bytes.length);
        buffer.put(bytes).flip();
        return buffer;
    }

    private static Map<String, byte[]> fixture() {
        Map<String, byte[]> files = new LinkedHashMap<>();
        files.put("ysm.json", ("{\"metadata\":{\"name\":\"模型\"},\"files\":{\"player\":{\"model\":{\"main\":\"models/main.json\"},"
                + "\"animation\":[\"animations/a.animation.json\"]}}}").getBytes(StandardCharsets.UTF_8));
        files.put("models/main.json", ("{\"minecraft:geometry\":[{\"description\":{\"texture_width\":16,\"texture_height\":16},"
                + "\"bones\":[{\"name\":\"root\",\"cubes\":[{\"origin\":[0,0,0],\"size\":[1,1,1],\"uv\":[0,0]}]}]}]}")
                .getBytes(StandardCharsets.UTF_8));
        files.put("animations/a.animation.json", "{\"animations\":{\"idle\":{\"loop\":true,\"animation_length\":1}}}"
                .getBytes(StandardCharsets.UTF_8));
        files.put("textures/skin.png", new byte[]{(byte) 0x89, 'P', 'N', 'G', 1, 2, 3, 4, 5, 6});
        return files;
    }

    private static byte[] zip(Map<String, byte[]> files, String prefix) throws Exception {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        try (ZipOutputStream zip = new ZipOutputStream(bytes, StandardCharsets.UTF_8)) {
            for (Map.Entry<String, byte[]> file : files.entrySet()) {
                zip.putNextEntry(new ZipEntry(prefix + file.getKey()));
                zip.write(file.getValue());
                zip.closeEntry();
            }
        }
        return bytes.toByteArray();
    }

    public static void main(String[] args) throws Exception {
        NativeApi.load(Path.of(args[0]));
        int[] version = NativeApi.omm_get_version();
        long runtime = NativeApi.omm_runtime_create(version[0], version[1], NativeApi.CPU_AUTO, 0, 0, 0, 0);
        int formats = NativeApi.omm_runtime_get_caps(runtime)[2];
        expect((formats & (1 << NativeApi.YSM_LEGACY_V3)) != 0, "parser formats advertised");

        // Raw entries; buffer positions must not move.
        Map<String, byte[]> files = fixture();
        String[] paths = files.keySet().toArray(new String[0]);
        ByteBuffer[] buffers = new ByteBuffer[paths.length];
        for (int index = 0; index < paths.length; index++) {
            buffers[index] = direct(files.get(paths[index]));
        }
        long asset = NativeApi.omm_ysm_parse_entries(runtime, paths, buffers, "fixture");
        expect(asset != 0, "raw asset handle");
        expect(buffers[0].position() == 0, "entry buffer position unchanged");
        long[] info = NativeApi.omm_asset_get_info(asset);
        expect(info[0] == NativeApi.YSM_RAW && info[3] == 1 && info[5] == 6 && info[7] == 1, "raw info counts");
        String metadata = new String(NativeApi.omm_asset_read_section(asset, NativeApi.SECTION_METADATA), StandardCharsets.UTF_8);
        expect(metadata.contains("\"name\":\"模型\""), "UTF-8 metadata round trip");
        ByteBuffer chunk = ByteBuffer.allocateDirect(4);
        int total = 0;
        for (long offset = 0; ; ) {
            int written = NativeApi.omm_asset_read_resource(asset, 3, offset, chunk, 0, 4);
            if (written == 0) {
                break;
            }
            total += written;
            offset += written;
        }
        expect(total == 10, "chunked resource read");
        NativeApi.omm_asset_release(asset);
        expectStatus(OmmNativeException.HANDLE, () -> NativeApi.omm_asset_get_info(asset), "released asset");

        // Nested ZIP root from a direct buffer window.
        byte[] archive = zip(files, "outer/model/");
        ByteBuffer window = ByteBuffer.allocateDirect(archive.length + 8);
        window.position(8);
        window.put(archive);
        int[] probe = NativeApi.omm_ysm_probe(window, 8, archive.length);
        expect(probe[0] == NativeApi.YSM_ZIP && probe[2] == 1, "zip probe");
        long zipped = NativeApi.omm_ysm_parse(runtime, window, 8, archive.length, "zip");
        String zipMetadata = new String(NativeApi.omm_asset_read_section(zipped, NativeApi.SECTION_METADATA), StandardCharsets.UTF_8);
        expect(zipMetadata.contains("\"zip_root\":\"outer/model\""), "recursive zip root");
        expectStatus(OmmNativeException.STATE, () -> NativeApi.omm_runtime_destroy(runtime), "runtime with live asset");
        NativeApi.omm_asset_release(zipped);

        // Error conversion.
        expectStatus(OmmNativeException.ARGUMENT,
                () -> NativeApi.omm_ysm_parse(runtime, ByteBuffer.wrap(archive), 0, archive.length, "heap"), "heap buffer rejected");
        expectStatus(OmmNativeException.ARGUMENT,
                () -> NativeApi.omm_ysm_parse(runtime, window, 8, archive.length + 1, "overrun"), "window overrun");
        ByteBuffer garbage = direct("garbage!".getBytes(StandardCharsets.UTF_8));
        expect(NativeApi.omm_ysm_probe(garbage, 0, 8)[0] == NativeApi.YSM_UNKNOWN, "unknown probe is not an exception");
        expectStatus(OmmNativeException.UNSUPPORTED, () -> NativeApi.omm_ysm_parse(runtime, garbage, 0, 8, "x"), "unknown format");
        expectStatus(OmmNativeException.FORMAT,
                () -> NativeApi.omm_ysm_parse_entries(runtime, new String[]{"../x.json"}, new ByteBuffer[]{direct(new byte[1])}, "x"),
                "path traversal");

        if (args.length > 1) {
            byte[] current = Files.readAllBytes(Path.of(args[1]));
            ByteBuffer currentInput = direct(current);
            expect(NativeApi.omm_ysm_probe(currentInput, 0, current.length)[0] == NativeApi.YSM_ASSET_CONTAINER,
                    "current container probe through JNI");
            long currentAsset = NativeApi.omm_ysm_parse(runtime, currentInput, 0, current.length, "current");
            expect(NativeApi.omm_asset_get_info(currentAsset)[0] == NativeApi.YSM_ASSET_CONTAINER,
                    "current container format through JNI");
            String currentAnimations = new String(NativeApi.omm_asset_read_section(currentAsset,
                    NativeApi.SECTION_ANIMATIONS), StandardCharsets.UTF_8);
            expect(currentAnimations.contains("fa0012") && currentAnimations.contains("q.blend"),
                    "portable programs survive JNI section transfer");
            NativeApi.omm_asset_release(currentAsset);
        }
        NativeApi.omm_runtime_destroy(runtime);
        if (failures != 0) {
            System.err.println(failures + " failure(s) on Java " + Runtime.version());
            System.exit(1);
        }
        System.out.println("JniParserTest: passed on Java " + Runtime.version());
    }
}
