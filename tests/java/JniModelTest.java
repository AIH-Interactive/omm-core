import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.file.Files;
import java.nio.file.Path;

import top.aihmc.omm.NativeApi;
import top.aihmc.omm.OmmNativeException;

/** Real JNI model/bake/pose/CPU-output test; no graphics context or third-party fixtures. */
public final class JniModelTest {
    private static int failures;

    private static ByteBuffer direct(int bytes) {
        return ByteBuffer.allocateDirect(bytes).order(ByteOrder.nativeOrder());
    }

    private static void expect(boolean condition, String message) {
        if (!condition) {
            System.err.println("FAIL: " + message);
            failures++;
        }
    }

    private static void status(int code, Runnable action) {
        try {
            action.run();
            expect(false, "expected status " + code);
        } catch (OmmNativeException exception) {
            expect(exception.code() == code, "status " + exception.code() + " != " + code);
        }
    }

    private static ByteBuffer matrix(double scale, double translation) {
        ByteBuffer result = direct(16 * Double.BYTES);
        for (int index = 0; index < 16; index++) {
            result.putDouble(index * Double.BYTES, index % 5 == 0 ? 1 : 0);
        }
        result.putDouble(0, scale);
        result.putDouble(12 * Double.BYTES, translation);
        return result;
    }

    private static long triangle(long runtime) {
        ByteBuffer parts = direct(NativeApi.PART_BYTES);
        parts.putInt(0, -1);
        ByteBuffer bind = matrix(1, 16);
        parts.position(8).put(bind).clear();
        ByteBuffer vertices = direct(3 * NativeApi.VERTEX_BYTES);
        for (int vertex = 0; vertex < 3; vertex++) {
            int offset = vertex * NativeApi.VERTEX_BYTES;
            vertices.putFloat(offset, vertex == 1 ? 16 : 0);
            vertices.putFloat(offset + 4, vertex == 2 ? 16 : 0);
            vertices.putFloat(offset + 20, 1);
        }
        ByteBuffer indices = direct(12);
        indices.asIntBuffer().put(new int[]{0, 1, 2});
        ByteBuffer surfaces = direct(NativeApi.SURFACE_BYTES);
        surfaces.putInt(4, 3).putInt(12, 1);
        ByteBuffer materials = direct(NativeApi.MATERIAL_BYTES);
        materials.putFloat(16, 1);
        return NativeApi.omm_model_create(runtime, NativeApi.GEOMETRY_FIGURA,
                1, parts, 3, vertices, 3, indices, 1, surfaces, 1, materials);
    }

    private static void generic(long runtime) {
        long model = triangle(runtime);
        int[] info = NativeApi.omm_model_get_info(model);
        expect(info.length == 6 && info[0] == NativeApi.GEOMETRY_FIGURA && info[1] == 1
                && info[2] == 3 && info[3] == 3 && info[4] == 1 && info[5] == 1, "model info layout");
        long instance = NativeApi.omm_instance_create(model);
        ByteBuffer ids = direct(4).putInt(0, 0);
        ByteBuffer parts = direct(NativeApi.PART_RESULT_BYTES);
        NativeApi.omm_instance_query_parts(instance, 1, ids, parts);
        expect(parts.getLong(0) == 0 && parts.getDouble(104) == 1, "Figura bind scaled once");
        ByteBuffer local = matrix(2, 3);
        ByteBuffer colors = direct(16);
        colors.asFloatBuffer().put(new float[]{1, 0.5f, 0.25f, 1});
        NativeApi.omm_instance_update(instance, 2, NativeApi.POSE_FULL, 1, ids, local,
                null, colors, null, null);
        NativeApi.omm_instance_query_parts(instance, 1, ids, parts);
        expect(parts.getLong(0) == 2 && parts.getDouble(104) == 3 && parts.getFloat(284) == 0.5f,
                "pose revision, canonical translation and color");
        ByteBuffer transformed = direct(3 * NativeApi.VERTEX_BYTES);
        long revision = NativeApi.omm_instance_transform_cpu(instance, NativeApi.CPU_AUTO, 3, transformed);
        expect(revision == 2 && transformed.getFloat(0) == 3
                && transformed.getFloat(NativeApi.VERTEX_BYTES) == 5, "actual CPU vertex transform");
        status(8, () -> NativeApi.omm_instance_transform_cpu(instance, NativeApi.CPU_AUTO, 2, direct(72)));
        status(OmmNativeException.ARGUMENT, () -> NativeApi.omm_instance_transform_cpu(instance,
                NativeApi.CPU_AUTO, 3, transformed.asReadOnlyBuffer()));
        ByteBuffer misaligned = direct(3 * NativeApi.VERTEX_BYTES + 1);
        misaligned.position(1);
        ByteBuffer slice = misaligned.slice();
        status(OmmNativeException.ARGUMENT, () -> NativeApi.omm_instance_transform_cpu(instance,
                NativeApi.CPU_AUTO, 3, slice));
        status(OmmNativeException.ARGUMENT, () -> NativeApi.omm_instance_query_parts(instance, 1,
                ids, ByteBuffer.allocate(NativeApi.PART_RESULT_BYTES)));
        status(OmmNativeException.ARGUMENT, () -> NativeApi.omm_instance_update(instance, 3,
                NativeApi.POSE_FULL, 1, ids, direct(8), null, null, null, null));
        local.putDouble(0, 0);
        status(OmmNativeException.ARGUMENT, () -> NativeApi.omm_instance_update(instance, 3,
                NativeApi.POSE_FULL, 1, ids, local, null, null, null, null));
        NativeApi.omm_instance_query_parts(instance, 1, ids, parts);
        expect(parts.getLong(0) == 2 && parts.getDouble(104) == 3, "failed update preserves revision and pose");
        NativeApi.omm_model_release(model);
        NativeApi.omm_instance_query_parts(instance, 1, ids, parts);
        NativeApi.omm_instance_release(instance);
        status(OmmNativeException.HANDLE, () -> NativeApi.omm_model_get_info(model));
        ByteBuffer sortItems = direct(3 * 32);
        sortItems.putDouble(16, 1).putLong(24, 0);
        sortItems.putDouble(32 + 16, 4).putLong(32 + 24, 2);
        sortItems.putDouble(64 + 16, -4).putLong(64 + 24, 1);
        ByteBuffer order = direct(12);
        NativeApi.omm_sort_back_to_front(runtime, 3, sortItems, direct(24), order);
        expect(order.getInt(0) == 2 && order.getInt(4) == 1 && order.getInt(8) == 0, "native distance/ordinal sort");
    }

    private static void baked(long runtime, Path fixture) throws Exception {
        byte[] data = Files.readAllBytes(fixture);
        ByteBuffer input = direct(data.length).put(data);
        long asset = NativeApi.omm_ysm_parse(runtime, input, 0, data.length, "own fixture");
        long model = NativeApi.omm_model_create_from_asset(runtime, asset, 0, null, 0, 0);
        NativeApi.omm_asset_release(asset);
        int[] info = NativeApi.omm_model_get_info(model);
        expect(info[0] == NativeApi.GEOMETRY_YSM && info[1] > 0 && info[2] > 0, "baked YSM model info");
        long instance = NativeApi.omm_instance_create(model);
        NativeApi.omm_model_release(model);
        long revision = NativeApi.omm_instance_transform_cpu(instance, NativeApi.CPU_AUTO,
                info[2], direct(info[2] * NativeApi.VERTEX_BYTES));
        expect(revision == 0, "baked model survives asset/model handle release");
        NativeApi.omm_instance_release(instance);
    }

    public static void main(String[] args) throws Exception {
        NativeApi.load(Path.of(args[0]));
        int[] version = NativeApi.omm_get_version();
        for (int threads : new int[]{1, 8, 16}) {
            long runtime = NativeApi.omm_runtime_create(version[0], version[1], NativeApi.CPU_AUTO,
                    threads, 0, 0, 0);
            generic(runtime);
            if (args.length > 1) {
                baked(runtime, Path.of(args[1]));
            }
            NativeApi.omm_runtime_destroy(runtime);
        }
        if (failures != 0) {
            throw new AssertionError(failures + " JNI model failures");
        }
        System.out.println("JniModelTest: passed on Java " + Runtime.version());
    }
}
