package top.aihmc.omnimorph.avatar.ysm;

import java.io.PrintStream;
import java.lang.reflect.Field;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

import com.google.gson.JsonObject;
import org.figuramc.figura.avatar.Avatar;
import top.aihmc.omnimorph.avatar.ysm.binary.YsmBinaryModel;
import top.aihmc.omnimorph.avatar.ysm.binary.YsmBinaryReader;
import top.aihmc.omnimorph.avatar.ysm.binary.YsmBinaryResourceMapper;
import top.aihmc.omnimorph.avatar.ysm.crypto.YsmDecodeBudget;
import top.aihmc.omnimorph.avatar.ysm.crypto.YsmLegacyDecoder;
import top.aihmc.omnimorph.avatar.ysm.crypto.YsmV3Decoder;
import top.aihmc.omnimorph.model.ysm.YsmGeometry;
import top.aihmc.omnimorph.model.ysm.YsmGeometryParser;
import top.aihmc.omnimorph.model.ysm.animation.YsmAnimationChannel;
import top.aihmc.omnimorph.model.ysm.animation.YsmAnimationClip;
import top.aihmc.omnimorph.model.ysm.animation.YsmAnimationEvent;
import top.aihmc.omnimorph.model.ysm.animation.YsmAnimationParser;
import top.aihmc.omnimorph.model.ysm.animation.YsmBoneAnimation;
import top.aihmc.omnimorph.model.ysm.animation.YsmKeyframe;
import top.aihmc.omnimorph.model.ysm.controller.YsmAnimationController;
import top.aihmc.omnimorph.model.ysm.controller.YsmAnimationControllerParser;
import top.aihmc.omnimorph.model.ysm.controller.YsmBlendTransition;
import top.aihmc.omnimorph.model.ysm.controller.YsmControllerAnimationRef;
import top.aihmc.omnimorph.model.ysm.controller.YsmControllerState;
import top.aihmc.omnimorph.model.ysm.controller.YsmControllerTransition;
import top.aihmc.omnimorph.molang.parser.ast.Expression;

/**
 * Reference side of the M2 normalization differential. Runs OMP's own manifest, geometry,
 * animation and controller parsers (Molang compilation replaced by a text-recording stub)
 * and prints the line format mirrored by tests/tools/ysm_normalized_dump.cpp.
 *
 * Usage: NormalizedDump <sample>...   (directories, legacy .ysm or v3 .ysm)
 */
public final class NormalizedDump {
    private final PrintStream out;

    private NormalizedDump(PrintStream out) {
        this.out = out;
    }

    public static void main(String[] args) throws Exception {
        boolean outputFile = args.length >= 2 && args[0].equals("--output");
        PrintStream out = outputFile
                ? new PrintStream(Files.newOutputStream(Path.of(args[1])), false, StandardCharsets.UTF_8)
                : new PrintStream(System.out, false, StandardCharsets.UTF_8);
        NormalizedDump dump = new NormalizedDump(out);
        for (int index = outputFile ? 2 : 0; index < args.length; index++) {
            Path path = Path.of(args[index]);
            String name = path.getFileName().toString();
            out.println("SAMPLE\t" + name);
            try {
                dump.sample(path, name);
            } catch (Exception failure) {
                out.println("ERROR\t" + failure.getClass().getSimpleName());
            }
            if (outputFile) {
                System.out.println("Normalized reference: " + name);
            }
        }
        out.flush();
        if (outputFile) {
            out.close();
        }
    }

    // ---- formatting (mirrored exactly by the native dump) ----

    private static String text(String value) {
        if (value == null) {
            return "<null>";
        }
        return value.replace("\\", "\\\\").replace("\t", "\\t").replace("\n", "\\n").replace("\r", "\\r");
    }

    private static String hex(float value) {
        return Integer.toHexString(Float.floatToRawIntBits(value));
    }

    private static String floats(float[] values) {
        StringBuilder builder = new StringBuilder();
        for (int index = 0; index < values.length; index++) {
            if (index != 0) {
                builder.append(',');
            }
            builder.append(hex(values[index]));
        }
        return builder.toString();
    }

    private static String expression(Expression expression) {
        return expression == null ? "-" : "e:" + text(expression.text());
    }

    private static String expressions(Expression[] values) {
        if (values == null) {
            return "-,-,-";
        }
        StringBuilder builder = new StringBuilder();
        for (int index = 0; index < 3; index++) {
            if (index != 0) {
                builder.append(',');
            }
            builder.append(expression(index < values.length ? values[index] : null));
        }
        return builder.toString();
    }

    private static String expressionList(List<Expression> values) {
        List<String> parts = new ArrayList<>();
        for (Expression value : values) {
            parts.add(expression(value));
        }
        return String.join("|", parts);
    }

    private static String strings(List<String> values) {
        List<String> parts = new ArrayList<>();
        for (String value : values) {
            parts.add(text(value));
        }
        return String.join("|", parts);
    }

    // ---- sample loading ----

    private static YsmDecodeBudget budget() {
        return new YsmDecodeBudget(YsmResourceLimits.DEFAULT, () -> false, 60000);
    }

    private void sample(Path path, String name) throws Exception {
        if (Files.isDirectory(path)) {
            try (YsmResourceSource source = YsmPackage.open(path)) {
                raw(source);
            }
            return;
        }
        byte[] bytes = Files.readAllBytes(path);
        boolean legacy = bytes.length >= 8 && bytes[0] == 'Y' && bytes[1] == 'S' && bytes[2] == 'G' && bytes[3] == 'P'
                && (ByteBuffer.wrap(bytes, 4, 4).order(ByteOrder.BIG_ENDIAN).getInt() == 1
                || ByteBuffer.wrap(bytes, 4, 4).order(ByteOrder.BIG_ENDIAN).getInt() == 2);
        if (legacy) {
            try (YsmMemoryResourceSource source = YsmLegacyDecoder.decode(bytes, name, budget())) {
                raw(source);
            }
            return;
        }
        YsmDecodeBudget budget = budget();
        byte[] payload = YsmV3Decoder.decode(bytes, budget);
        try (YsmBinaryModel model = YsmBinaryReader.readFilePayload(payload, budget);
             YsmBinaryResourceMapper.Mapping mapping = YsmBinaryResourceMapper.map(model, name, budget)) {
            manifest(mapping.manifest());
            for (String geometryPath : geometryPaths(mapping.manifest())) {
                geometry(geometryPath, mapping.geometries().get(geometryPath));
            }
            motion(mapping.resources(), mapping.manifest());
        }
    }

    private static List<String> geometryPaths(YsmManifest manifest) {
        List<String> paths = new ArrayList<>();
        if (!manifest.mainModelPath().isBlank()) {
            paths.add(manifest.mainModelPath());
        }
        if (!manifest.armModelPath().isBlank()) {
            paths.add(manifest.armModelPath());
        }
        return paths;
    }

    private void raw(YsmResourceSource source) throws Exception {
        YsmManifest manifest = YsmManifestReader.read(source);
        manifest(manifest);
        for (String geometryPath : geometryPaths(manifest)) {
            geometry(geometryPath, YsmGeometryParser.parse(source.readString(geometryPath)));
        }
        motion(source, manifest);
    }

    private void manifest(YsmManifest manifest) {
        out.println("MANIFEST\t" + manifest.kind() + "\t" + text(manifest.name()) + "\t" + text(manifest.description())
                + "\t" + strings(List.of(manifest.authors())) + "\t" + text(manifest.mainModelPath()) + "\t"
                + text(manifest.armModelPath()) + "\t" + text(manifest.defaultTexture()) + "\t" + manifest.free());
        for (YsmTextureOption texture : manifest.textures()) {
            out.println("TEXTURE\t" + text(texture.id()) + "\t" + text(texture.displayName()) + "\t" + text(texture.path())
                    + "\t" + text(texture.normalPath()) + "\t" + text(texture.specularPath()));
        }
        out.println("ANIMATIONS\t" + strings(manifest.animationPaths()));
        out.println("CONTROLLERS\t" + strings(manifest.animationControllerPaths()));
        YsmResourceIndex index = manifest.resourceIndex();
        out.println("FUNCTIONS\t" + strings(index.functionPaths()));
        out.println("SOUNDS\t" + strings(index.soundPaths()));
        out.println("LANGUAGES\t" + strings(index.languagePaths()));
        out.println("PROJECTILES\t" + strings(index.projectilePaths()));
        out.println("VEHICLES\t" + strings(index.vehiclePaths()));
        out.println("IMAGES\t" + text(index.iconPath()) + "\t" + text(index.backgroundPath()));
        for (String warning : index.warnings()) {
            out.println("WARNING\t" + text(warning));
        }
    }

    private void geometry(String path, YsmGeometry geometry) {
        out.println("GEOMETRY\t" + text(path) + "\t" + geometry.textureWidth + "\t" + geometry.textureHeight + "\t"
                + geometry.bones.size());
        for (YsmGeometry.Bone bone : geometry.bones.values()) {
            out.println("BONE\t" + text(bone.name) + "\t" + text(bone.parentName) + "\t" + floats(bone.pivot) + "\t"
                    + floats(bone.rotation) + "\t" + bone.cubes.size());
            for (YsmGeometry.Cube cube : bone.cubes) {
                out.println("CUBE\t" + cube.quads.size());
                for (YsmGeometry.Quad quad : cube.quads) {
                    out.println("QUAD\t" + floats(quad.positions()) + "\t" + floats(quad.uvs()) + "\t" + floats(quad.normal()));
                }
            }
        }
    }

    private static boolean mergeMultiline(YsmResourceSource source) throws Exception {
        if (!source.exists("ysm.json")) {
            return false;
        }
        JsonObject root = YsmJson.parseObject(source.readString("ysm.json"));
        return Boolean.parseBoolean(YsmJson.string(YsmJson.obj(root, "properties"), "merge_multiline_expr", "false"));
    }

    private void motion(YsmResourceSource source, YsmManifest manifest) throws Exception {
        boolean merge = mergeMultiline(source);
        List<String> firstPerson = YsmManifestReader.firstPersonAnimationPaths(source);
        for (String path : manifest.animationPaths()) {
            String json = source.readString(path);
            boolean first = firstPerson.contains(path);
            try {
                JsonObject root = com.google.gson.JsonParser.parseString(json).getAsJsonObject();
                first = first || root.has("ysm_animation_type") && root.get("ysm_animation_type").getAsInt() == 11;
            } catch (RuntimeException ignored) {
                // Same tolerance as YsmModelRuntime.isFirstPersonAnimation.
            }
            out.println("ANIMFILE\t" + text(path) + "\t" + first);
            Map<String, YsmAnimationClip> clips = new TreeMap<>(
                    YsmAnimationParser.parse(json, true, Avatar.getMolangEngine(), merge));
            for (YsmAnimationClip clip : clips.values()) {
                clip(clip);
            }
        }
        for (String path : manifest.animationControllerPaths()) {
            out.println("CTRLFILE\t" + text(path));
            Map<String, YsmAnimationController> controllers = YsmAnimationControllerParser.parse(
                    source.readString(path), Avatar.getMolangEngine(), null, merge);
            for (YsmAnimationController controller : controllers.values()) {
                controller(controller);
            }
        }
    }

    private void clip(YsmAnimationClip clip) {
        boolean unbounded = clip.length == Float.MAX_VALUE;
        out.println("CLIP\t" + text(clip.name) + "\t" + (unbounded ? "unbounded" : hex(clip.length)) + "\t"
                + clip.loopMode + "\t" + hex(clip.blendWeight) + "\t" + expressionList(clip.blendWeightExpressions));
        for (YsmBoneAnimation bone : new TreeMap<>(clip.boneAnimations).values()) {
            out.println("BONEANIM\t" + text(bone.boneName));
            channel("position", bone.position);
            channel("rotation", bone.rotation);
            channel("scale", bone.scale);
        }
        for (int index = 0; index < clip.events.size(); index++) {
            YsmAnimationEvent event = clip.events.get(index);
            List<String> params = new ArrayList<>();
            for (Map.Entry<String, String> param : new TreeMap<>(event.params()).entrySet()) {
                params.add(text(param.getKey()) + "=" + text(param.getValue()));
            }
            out.println("EVENT\t" + index + "\t" + hex(event.time()) + "\t" + event.type() + "\t" + text(event.value())
                    + "\t" + String.join("|", params));
        }
    }

    private void channel(String type, YsmAnimationChannel channel) {
        if (channel == null) {
            out.println("CHANNEL\t" + type + "\tnone");
            return;
        }
        if (channel.keyframes.isEmpty()) {
            out.println("CHANNEL\t" + type + "\tstatic\t" + floats(channel.staticValue) + "\t"
                    + expressions(channel.staticExpressions));
            return;
        }
        out.println("CHANNEL\t" + type + "\tkeyframes\t" + channel.keyframes.size());
        for (YsmKeyframe keyframe : channel.keyframes) {
            out.println("KF\t" + hex(keyframe.time) + "\t" + text(keyframe.interpolation) + "\t" + floats(keyframe.value)
                    + "\t" + expressions(keyframe.expressions) + "\t" + floats(keyframe.preValue) + "\t"
                    + expressions(keyframe.preExpressions));
        }
    }

    private static float[] blendArray(YsmBlendTransition transition, String field) throws Exception {
        Field reflected = YsmBlendTransition.class.getDeclaredField(field);
        reflected.setAccessible(true);
        return (float[]) reflected.get(transition);
    }

    private void controller(YsmAnimationController controller) throws Exception {
        out.println("CTRL\t" + text(controller.name()) + "\t" + text(controller.initialState()));
        for (YsmControllerState state : controller.states().values()) {
            List<String> animations = new ArrayList<>();
            for (YsmControllerAnimationRef animation : state.animations()) {
                animations.add(text(animation.animation()) + "=" + expression(animation.condition()));
            }
            List<String> transitions = new ArrayList<>();
            for (YsmControllerTransition transition : state.transitions()) {
                transitions.add(text(transition.targetState()) + "=" + expression(transition.condition()));
            }
            YsmBlendTransition curve = state.blendCurve();
            float[] times = blendArray(curve, "times");
            String blend = times.length == 0 ? "linear:" + hex(curve.duration())
                    : "curve:" + floats(times) + ":" + floats(blendArray(curve, "incoming"));
            out.println("STATE\t" + text(state.name()) + "\t" + String.join("|", animations) + "\t"
                    + String.join("|", transitions) + "\t" + expressionList(state.onEntry()) + "\t"
                    + expressionList(state.onExit()) + "\t" + hex(state.blendTransition()) + "\t" + blend + "\t"
                    + state.blendViaShortestPath() + "\t" + strings(state.soundEffects()));
        }
    }
}
