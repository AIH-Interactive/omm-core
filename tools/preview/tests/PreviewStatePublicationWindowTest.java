package top.aihmc.omm.preview;

import static org.lwjgl.glfw.GLFW.*;

import java.nio.FloatBuffer;
import java.nio.file.Path;
import org.lwjgl.opengl.GL;
import org.lwjgl.system.MemoryUtil;

/** Exercises real animation states and delayed consumers on a visible Windows GL context. */
public final class PreviewStatePublicationWindowTest {
    private static void compare(FloatBuffer actual, FloatBuffer expected, String label) {
        for (int value = 0; value < expected.capacity(); value++) {
            float left = actual.get(value);
            float right = expected.get(value);
            if (!Float.isFinite(left) || Math.abs(left - right) > 0.00005f) {
                throw new AssertionError(label + " value " + value + ": " + left + " != " + right);
            }
        }
    }

    private static void check(String configPath, String library) throws Exception {
        var config = PreviewConfig.load(new String[] {"--config", configPath});
        config.execution.nativeLibrary = Path.of(library).toAbsolutePath().toString();
        config.execution.threads = 1;
        config.settings.enabled = false;
        var mesh = PreviewMesh.load(config);
        int values = mesh.skeleton.names.length * 16;
        FloatBuffer expected = MemoryUtil.memAllocFloat(values);
        FloatBuffer scratch = MemoryUtil.memAllocFloat(values);
        try (var animation = new PreviewAnimation(mesh, config);
             var bridge = new PreviewNativeVertices(mesh, config)) {
            var javaState = animation.newState();
            var first = animation.newState();
            var second = animation.newState();
            for (int frame = 0; frame < 3; frame++) {
                float time = frame / 60.0f;
                javaState.sample(frame, time);
                javaState.updateBones();
                javaState.writePalette(expected);
                first.sample(frame, time);
                first.updateBonesForRender(bridge);
                FloatBuffer published = first.paletteBuffer(scratch);
                // Another instance uses the shared bridge before the first instance is consumed.
                second.sample(frame, time + 0.25f);
                second.updateBonesForRender(bridge);
                compare(published, expected, "instance-owned publication");
                first.paletteMatrices();
                first.writePalette(scratch);
                compare(scratch, expected, "deferred palette objects");
                first.recordRenderedPivots();
                PreviewCpuVerticesTest.check(mesh, published, bridge);
                first.postRender(time);
                javaState.postRender(time);
                second.postRender(time + 0.25f);
            }
            System.out.println("State publication PASS: " + mesh.label + "; "
                + animation.description() + "; paired states, deferred objects, CPU/GPU parity");
        } finally {
            MemoryUtil.memFree(expected);
            MemoryUtil.memFree(scratch);
        }
    }

    public static void main(String[] args) throws Exception {
        if (!glfwInit()) {
            throw new IllegalStateException("GLFW initialization failed");
        }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
        long window = glfwCreateWindow(640, 480, "Animation state publication parity", 0, 0);
        if (window == 0) {
            glfwTerminate();
            throw new IllegalStateException("Visible publication window failed");
        }
        try {
            glfwMakeContextCurrent(window);
            GL.createCapabilities();
            glfwShowWindow(window);
            glfwPollEvents();
            Thread.sleep(200);
            for (int index = 1; index < args.length; index++) {
                check(args[index], args[0]);
            }
        } finally {
            glfwDestroyWindow(window);
            glfwTerminate();
        }
    }
}
