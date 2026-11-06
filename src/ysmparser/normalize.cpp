#include "ysmparser/normalize.hpp"
#include "abi/parallel.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace omm::ysm {

namespace {

// Thrown where the Java parsers raise an unchecked exception inside their try block.
struct ParseAbort {
    std::string message;
};

[[noreturn]] void abort_parse(std::string message) {
    throw ParseAbort{std::move(message)};
}

bool is_blank(const std::string &text) {
    return std::all_of(text.begin(), text.end(), [](char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
    });
}

// JsonElement.getAsFloat with the reference's exception behaviour.
float require_float(const json::Value &value, const char *what) {
    const auto parsed = value.as_float();
    if (!parsed) {
        abort_parse(std::string("NumberFormatException: ") + what);
    }
    return *parsed;
}

// JsonElement.getAsString on a non-primitive throws in Gson.
std::string require_string(const json::Value &value, const char *what) {
    if (!value.is_primitive()) {
        abort_parse(std::string("UnsupportedOperationException: ") + what);
    }
    return value.as_string();
}

// ---------------------------------------------------------------------------
// JOML 1.10.8 Matrix4f/Vector4f/Vector3f semantics, operation for operation.
// JOML routes products through org.joml.Math.fma, which is a plain a * b + c (two
// roundings) unless -Djoml.useMathFma=true; jfma reproduces that, including the nesting
// order. Property flags select the same code paths JOML takes (identity / translation-only /
// general affine). Storage is column-major: m[column][row]. Requires no FP contraction.
// ---------------------------------------------------------------------------
// org.joml.Math.fma with the default Options (USE_MATH_FMA = false).
inline float jfma(float a, float b, float c) {
    const float product = a * b;
    return product + c;
}

struct Matrix4 {
    enum class Kind { identity, translation, affine };

    float m[4][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
    Kind kind = Kind::identity;

    void translate(float x, float y, float z) {
        if (kind == Kind::identity) {
            m[3][0] = x; // Matrix4f.translation(x, y, z)
            m[3][1] = y;
            m[3][2] = z;
            kind = Kind::translation;
            return;
        }
        for (int row = 0; row < 4; ++row) { // translateGeneric
            m[3][row] = jfma(m[0][row], x, jfma(m[1][row], y, jfma(m[2][row], z, m[3][row])));
        }
    }

    // rotationX/Y/Z(angle) followed by setTranslation, used on identity or translation-only matrices.
    void set_rotation(int axis, float sin, float cos) {
        const float tx = m[3][0];
        const float ty = m[3][1];
        const float tz = m[3][2];
        *this = Matrix4{};
        if (axis == 0) {
            m[1][1] = cos;
            m[1][2] = sin;
            m[2][1] = -sin;
            m[2][2] = cos;
        } else if (axis == 1) {
            m[0][0] = cos;
            m[0][2] = -sin;
            m[2][0] = sin;
            m[2][2] = cos;
        } else {
            m[0][0] = cos;
            m[0][1] = sin;
            m[1][0] = -sin;
            m[1][1] = cos;
        }
        m[3][0] = tx;
        m[3][1] = ty;
        m[3][2] = tz;
        kind = Kind::affine;
    }

    void rotate_x(float angle) {
        const float sin = java_sin(angle);
        const float cos = cos_from_sin(sin, angle);
        if (kind != Kind::affine) {
            set_rotation(0, sin, cos);
            return;
        }
        for (int row = 0; row < 4; ++row) { // rotateXInternal
            const float l1 = m[1][row];
            const float l2 = m[2][row];
            m[2][row] = jfma(l1, -sin, l2 * cos);
            m[1][row] = jfma(l1, cos, l2 * sin);
        }
    }

    void rotate_y(float angle) {
        const float sin = java_sin(angle);
        const float cos = cos_from_sin(sin, angle);
        if (kind != Kind::affine) {
            set_rotation(1, sin, cos);
            return;
        }
        for (int row = 0; row < 4; ++row) { // rotateYInternal
            const float l0 = m[0][row];
            const float l2 = m[2][row];
            m[0][row] = jfma(l0, cos, l2 * -sin);
            m[2][row] = jfma(l0, sin, l2 * cos);
        }
    }

    void rotate_z(float angle) {
        const float sin = java_sin(angle);
        const float cos = cos_from_sin(sin, angle);
        if (kind != Kind::affine) {
            set_rotation(2, sin, cos);
            return;
        }
        for (int row = 0; row < 4; ++row) { // rotateTowardsXY(sin, cos)
            const float l0 = m[0][row];
            const float l1 = m[1][row];
            m[0][row] = jfma(l0, cos, l1 * sin);
            m[1][row] = jfma(l0, -sin, l1 * cos);
        }
    }

    // JOML Math.sin(float): (float) java.lang.Math.sin(double).
    static float java_sin(float angle) {
        return static_cast<float>(std::sin(static_cast<double>(angle)));
    }

    // JOML Math.cosFromSinInternal (non-FASTMATH path).
    static float cos_from_sin(float sin, float angle) {
        constexpr float kPi = 3.14159265358979323846f;     // (float) Math.PI
        constexpr float kTwoPi = kPi * 2.0f;               // PI_TIMES_2_f
        constexpr float kHalfPi = 1.57079632679489661923f; // (float) (PI * 0.5)
        const float cos = static_cast<float>(std::sqrt(static_cast<double>(1.0f - sin * sin)));
        const float a = angle + kHalfPi;
        float b = a - static_cast<float>(static_cast<int>(a / kTwoPi)) * kTwoPi;
        if (b < 0.0f) {
            b = kTwoPi + b;
        }
        return b >= kPi ? -cos : cos;
    }

    // Vector4f.mul(Matrix4f) with w = 1.
    void transform_point(const float in[3], float out[3]) const {
        if (kind == Kind::identity) {
            out[0] = in[0];
            out[1] = in[1];
            out[2] = in[2];
            return;
        }
        if (kind == Kind::translation) { // mulTranslation
            out[0] = in[0] + m[3][0];
            out[1] = in[1] + m[3][1];
            out[2] = in[2] + m[3][2];
            return;
        }
        for (int row = 0; row < 3; ++row) { // mulAffine
            out[row] = jfma(m[0][row], in[0], jfma(m[1][row], in[1], jfma(m[2][row], in[2], m[3][row] * 1.0f)));
        }
    }

    // Matrix4f.normal (orthonormal: upper 3x3), Vector3f.mul(Matrix3f), then normalize().
    void transform_normal(const float in[3], float out[3]) const {
        for (int row = 0; row < 3; ++row) {
            out[row] = jfma(m[0][row], in[0], jfma(m[1][row], in[1], m[2][row] * in[2]));
        }
        const float squared = jfma(out[0], out[0], jfma(out[1], out[1], out[2] * out[2]));
        const float scalar = 1.0f / static_cast<float>(std::sqrt(static_cast<double>(squared))); // Math.invsqrt
        for (int row = 0; row < 3; ++row) {
            out[row] = out[row] * scalar;
        }
    }
};

float to_radians(float degrees) {
    return static_cast<float>(static_cast<double>(degrees) * (3.14159265358979323846 / 180.0));
}

// ---------------------------------------------------------------------------
// Geometry (YsmGeometryParser)
// ---------------------------------------------------------------------------
json::Value array_of(const json::Value &parent, std::string_view key) {
    const json::Value element = parent.get(key);
    return element.is_array() ? element : json::Value();
}

std::string geo_string(const json::Value &parent, std::string_view key, const std::string &fallback) {
    const json::Value element = parent.get(key);
    return element.is_primitive() ? element.as_string() : fallback;
}

int32_t geo_int(const json::Value &parent, std::string_view key, int32_t fallback) {
    const json::Value element = parent.get(key);
    if (!element.is_primitive()) {
        return fallback;
    }
    const auto value = element.as_int();
    if (!value) {
        abort_parse("NumberFormatException: integer");
    }
    return *value;
}

float geo_float(const json::Value &parent, std::string_view key, float fallback) {
    const json::Value element = parent.get(key);
    return element.is_primitive() ? require_float(element, "float") : fallback;
}

bool geo_bool(const json::Value &parent, std::string_view key, bool fallback) {
    const json::Value element = parent.get(key);
    return element.is_primitive() ? element.as_bool() : fallback;
}

std::array<float, 3> geo_vec3(const json::Value &parent, std::string_view key, float x, float y, float z) {
    const json::Value element = parent.get(key);
    if (element.is_array()) {
        const auto items = element.items();
        if (items.size() >= 3) {
            return {require_float(items[0], "vec3"), require_float(items[1], "vec3"), require_float(items[2], "vec3")};
        }
    }
    return {x, y, z};
}

std::array<float, 2> geo_vec2(const json::Value &parent, std::string_view key) {
    const json::Value element = parent.get(key);
    if (element.is_array()) {
        const auto items = element.items();
        if (items.size() >= 2) {
            return {require_float(items[0], "vec2"), require_float(items[1], "vec2")};
        }
    }
    return {0.0f, 0.0f};
}

struct FaceUv {
    std::array<float, 2> uv{};
    std::array<float, 2> size{};
};

struct BakeContext {
    float x, y, z, w, h, d;
    float texture_width, texture_height;
    bool mirror;
    Matrix4 matrix;
};

void bake_face(std::vector<Quad> &quads, const std::optional<FaceUv> &face, const char *face_type,
               const BakeContext &context, const float raw_normal[3]) {
    if (!face) {
        return;
    }
    float u0 = face->uv[0] / context.texture_width;
    float v0 = face->uv[1] / context.texture_height;
    float u1 = (face->uv[0] + face->size[0]) / context.texture_width;
    const float v1 = (face->uv[1] + face->size[1]) / context.texture_height;
    if (!context.mirror) {
        std::swap(u0, u1);
    }
    const float x1 = context.x / 16.0f, x2 = (context.x + context.w) / 16.0f;
    const float y1 = context.y / 16.0f, y2 = (context.y + context.h) / 16.0f;
    const float z1 = context.z / 16.0f, z2 = (context.z + context.d) / 16.0f;
    const float p[8][3] = {
        {x1, y1, z1}, {x1, y1, z2}, {x1, y2, z1}, {x1, y2, z2}, {x2, y1, z1}, {x2, y1, z2}, {x2, y2, z1}, {x2, y2, z2},
    };
    int order[4];
    const std::string type(face_type);
    if (type == "west") {
        order[0] = 3;
        order[1] = 2;
        order[2] = 0;
        order[3] = 1;
    } else if (type == "east") {
        order[0] = 6;
        order[1] = 7;
        order[2] = 5;
        order[3] = 4;
    } else if (type == "north") {
        order[0] = 2;
        order[1] = 6;
        order[2] = 4;
        order[3] = 0;
    } else if (type == "south") {
        order[0] = 7;
        order[1] = 3;
        order[2] = 1;
        order[3] = 5;
    } else if (type == "up") {
        order[0] = 3;
        order[1] = 7;
        order[2] = 6;
        order[3] = 2;
    } else {
        order[0] = 0;
        order[1] = 4;
        order[2] = 5;
        order[3] = 1; // down
    }
    Quad quad;
    context.matrix.transform_normal(raw_normal, quad.normal.data());
    for (int vertex = 0; vertex < 4; ++vertex) {
        context.matrix.transform_point(p[order[vertex]], quad.positions.data() + vertex * 3);
    }
    quad.uvs = {u0, v0, u1, v0, u1, v1, u0, v1};
    quads.push_back(quad);
}

std::optional<FaceUv> face_uv(const json::Value &object, const char *name) {
    const json::Value element = object.get(name);
    if (!element.is_object()) {
        return std::nullopt;
    }
    return FaceUv{geo_vec2(element, "uv"), geo_vec2(element, "uv_size")};
}

GeometryCube bake_cube(const json::Value &cube, bool bone_mirror, float bone_inflate, float texture_width,
                       float texture_height) {
    const float inflate = geo_float(cube, "inflate", bone_inflate);
    const bool mirror = geo_bool(cube, "mirror", bone_mirror);
    const auto origin = geo_vec3(cube, "origin", 0, 0, 0);
    const auto size = geo_vec3(cube, "size", 0, 0, 0);

    BakeContext context{};
    context.x = -origin[0] - size[0] - inflate;
    context.y = origin[1] - inflate;
    context.z = origin[2] - inflate;
    context.w = size[0] + inflate * 2.0f;
    context.h = size[1] + inflate * 2.0f;
    context.d = size[2] + inflate * 2.0f;
    context.texture_width = texture_width;
    context.texture_height = texture_height;
    context.mirror = mirror;
    if (cube.has("rotation") || cube.has("pivot")) {
        const auto pivot = geo_vec3(cube, "pivot", 0, 0, 0);
        const auto rotation = geo_vec3(cube, "rotation", 0, 0, 0);
        context.matrix.translate(-pivot[0] / 16.0f, pivot[1] / 16.0f, pivot[2] / 16.0f);
        context.matrix.rotate_z(to_radians(rotation[2]));
        context.matrix.rotate_y(-to_radians(rotation[1]));
        context.matrix.rotate_x(-to_radians(rotation[0]));
        context.matrix.translate(pivot[0] / 16.0f, -pivot[1] / 16.0f, -pivot[2] / 16.0f);
    }

    GeometryCube result;
    const json::Value uv = cube.get("uv");
    if (!uv.present()) {
        return result;
    }
    std::optional<FaceUv> faces[6]; // north, south, east, west, up, down (uv source keys)
    if (uv.is_object()) {
        faces[0] = face_uv(uv, "north");
        faces[1] = face_uv(uv, "south");
        faces[2] = face_uv(uv, mirror ? "west" : "east");
        faces[3] = face_uv(uv, mirror ? "east" : "west");
        faces[4] = face_uv(uv, "up");
        faces[5] = face_uv(uv, "down");
    } else if (uv.is_array() && uv.items().size() >= 2) {
        const auto items = uv.items();
        const float u = require_float(items[0], "box uv");
        const float v = require_float(items[1], "box uv");
        const float dx = std::floor(size[0]);
        const float dy = std::floor(size[1]);
        const float dz = std::floor(size[2]);
        const FaceUv north{{u + dz, v + dz}, {dx, dy}};
        const FaceUv south{{u + dz + dx + dz, v + dz}, {dx, dy}};
        const FaceUv east{{u, v + dz}, {dz, dy}};
        const FaceUv west{{u + dz + dx, v + dz}, {dz, dy}};
        const FaceUv up{{u + dz, v}, {dx, dz}};
        const FaceUv down{{u + dz + dx, v + dz}, {dx, -dz}};
        faces[0] = north;
        faces[1] = south;
        faces[2] = mirror ? west : east;
        faces[3] = mirror ? east : west;
        faces[4] = up;
        faces[5] = down;
    } else {
        return result;
    }
    const float normals[6][3] = {{0, 0, -1}, {0, 0, 1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
    const char *types[6] = {"north", "south", "east", "west", "up", "down"};
    for (int face = 0; face < 6; ++face) {
        bake_face(result.quads, faces[face], types[face], context, normals[face]);
    }
    return result;
}

// ---------------------------------------------------------------------------
// Animation values (YsmAnimationParser.parseValueOrExpression)
// ---------------------------------------------------------------------------
ComponentVector default_vector(const std::string &type) {
    const float base = type == "scale" ? 1.0f : 0.0f;
    ComponentVector vector;
    for (Component &component : vector) {
        component.value = base;
    }
    return vector;
}

// value == nullopt when the reference's ParseResult.value is null.
struct ParsedValue {
    std::optional<std::array<float, 3>> value;
    std::array<std::optional<std::string>, 3> expressions;
};

std::optional<std::string> expression_text(const std::string &text) {
    if (is_blank(text)) {
        return std::nullopt; // compileExpression returns null for blank input
    }
    return text;
}

ParsedValue parse_value(const json::Value &element, const std::string &type) {
    ParsedValue result;
    if (element.is_primitive()) {
        const std::string text = element.as_string();
        if (const auto number = json::parse_java_float(text)) {
            result.value = std::array<float, 3>{*number, *number, *number};
            return result;
        }
        const auto expression = expression_text(text);
        result.expressions = {expression, expression, expression};
        return result;
    }
    if (element.is_array()) {
        const float base = type == "scale" ? 1.0f : 0.0f;
        std::array<float, 3> value = {base, base, base};
        const auto items = element.items();
        for (size_t index = 0; index < std::min<size_t>(3, items.size()); ++index) {
            if (!items[index].is_primitive()) {
                continue;
            }
            const std::string text = items[index].as_string();
            if (const auto number = json::parse_java_float(text)) {
                value[index] = *number;
            } else {
                result.expressions[index] = expression_text(text);
            }
        }
        result.value = value;
    }
    return result;
}

ComponentVector to_components(const ParsedValue &parsed, const ComponentVector &fallback) {
    ComponentVector vector = fallback;
    for (int axis = 0; axis < 3; ++axis) {
        if (parsed.value) {
            vector[axis].value = (*parsed.value)[axis];
        }
        vector[axis].expression = parsed.expressions[axis];
    }
    return vector;
}

std::optional<Channel> parse_channel(const json::Value &element, const std::string &type) {
    if (!element.present()) {
        return std::nullopt;
    }
    const ComponentVector fallback = default_vector(type);
    if (element.is_primitive() || element.is_array()) {
        Channel channel;
        channel.constant = to_components(parse_value(element, type), fallback);
        return channel;
    }
    if (!element.is_object()) {
        return std::nullopt;
    }
    Channel channel;
    channel.constant = fallback;
    channel.keyframed = true;
    for (const auto &[time_text, keyframe_value] : element.members()) {
        const auto time = json::parse_java_float(time_text);
        if (!time) {
            continue;
        }
        json::Value actual = keyframe_value;
        json::Value pre_value;
        std::string interpolation = "linear";
        if (keyframe_value.is_object()) {
            if (keyframe_value.has("pre") && keyframe_value.has("post")) {
                pre_value = keyframe_value.get("pre");
                actual = keyframe_value.get("post");
            } else if (keyframe_value.has("vector")) {
                actual = keyframe_value.get("vector");
            } else if (keyframe_value.has("post")) {
                actual = keyframe_value.get("post");
            } else if (keyframe_value.has("pre")) {
                actual = keyframe_value.get("pre");
            }
            if (keyframe_value.has("lerp_mode")) {
                interpolation = require_string(keyframe_value.get("lerp_mode"), "lerp_mode");
            } else if (keyframe_value.has("easing")) {
                interpolation = require_string(keyframe_value.get("easing"), "easing");
            }
        }
        const ParsedValue post = parse_value(actual, type);
        const ParsedValue pre = pre_value.present() ? parse_value(pre_value, type) : post;
        Keyframe keyframe;
        keyframe.time = *time;
        keyframe.interpolation = interpolation;
        keyframe.post = to_components(post, fallback);
        keyframe.pre = to_components(pre, fallback);
        channel.keyframes.push_back(std::move(keyframe));
    }
    std::stable_sort(channel.keyframes.begin(), channel.keyframes.end(),
                     [](const Keyframe &a, const Keyframe &b) { return a.time < b.time; });
    for (size_t index = 0; index < channel.keyframes.size(); ++index) {
        channel.keyframes[index].ordinal = static_cast<uint32_t>(index);
    }
    return channel;
}

float last_keyframe_time(const json::Value &channel) {
    float last = 0.0f;
    if (!channel.is_object()) {
        return last;
    }
    for (const auto &member : channel.members()) {
        if (const auto time = json::parse_java_float(member.first)) {
            last = std::max(last, *time);
        }
    }
    return last;
}

float derived_length(const json::Value &bones) {
    float longest = 0.0f;
    if (bones.is_object()) {
        for (const auto &bone : bones.members()) {
            if (!bone.second.is_object()) {
                continue;
            }
            for (const char *channel : {"rotation", "position", "scale"}) {
                longest = std::max(longest, last_keyframe_time(bone.second.get(channel)));
            }
        }
    }
    return longest == 0.0f ? std::numeric_limits<float>::max() : longest;
}

LoopMode loop_mode(const std::string &value) {
    std::string lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; });
    if (lower == "true" || lower == "loop") {
        return LoopMode::loop;
    }
    if (lower == "hold" || lower == "hold_on_last_frame") {
        return LoopMode::hold_on_last_frame;
    }
    return LoopMode::once;
}

void add_event(AnimationClip &clip, float time, const std::string &type, const json::Value &value) {
    if (!value.present() || value.is_null()) {
        return;
    }
    std::string text;
    std::vector<std::pair<std::string, std::string>> params;
    if (value.is_primitive()) {
        text = value.as_string();
    } else if (value.is_object()) {
        for (const auto &member : value.members()) {
            if (!member.second.present() || member.second.is_null()) {
                continue;
            }
            params.emplace_back(member.first,
                                member.second.is_primitive() ? member.second.as_string() : member.second.to_json());
        }
        bool found = false;
        for (const char *key : {"effect", "particle", "sound", "name", "event", "id"}) {
            const json::Value candidate = value.get(key);
            if (candidate.is_primitive()) {
                text = candidate.as_string();
                found = true;
                break;
            }
        }
        if (!found || is_blank(text)) {
            text = value.to_json();
        }
    } else {
        text = value.to_json();
    }
    if (!is_blank(text)) {
        clip.events.push_back(AnimationEvent{time, type, text, std::move(params), 0});
    }
}

void parse_events(const json::Value &element, const std::string &type, AnimationClip &clip, bool merge_multiline) {
    if (!element.is_object()) {
        return;
    }
    for (const auto &[time_text, value] : element.members()) {
        const auto time = json::parse_java_float(time_text);
        if (!time) {
            continue;
        }
        if (!value.present() || value.is_null()) {
            continue;
        }
        if (value.is_array() && merge_multiline && type == "timeline") {
            std::string text;
            for (const json::Value &line : value.items()) {
                if (!text.empty()) {
                    text += '\n';
                }
                text += require_string(line, "timeline line");
            }
            if (!is_blank(text)) {
                clip.events.push_back(AnimationEvent{*time, type, text, {}, 0});
            }
        } else if (value.is_array()) {
            for (const json::Value &child : value.items()) {
                add_event(clip, *time, type, child);
            }
        } else {
            add_event(clip, *time, type, value);
        }
    }
}

// ---------------------------------------------------------------------------
// Controllers (YsmAnimationControllerParser)
// ---------------------------------------------------------------------------
std::string ctrl_string(const json::Value &element, const std::string &fallback) {
    if (!element.present() || element.is_null()) {
        return fallback;
    }
    return element.is_primitive() ? element.as_string() : element.to_json();
}

json::Value first_of(const json::Value &object, std::initializer_list<const char *> keys) {
    for (const char *key : keys) {
        const json::Value element = object.get(key);
        if (element.present() && !element.is_null()) {
            return element;
        }
    }
    return json::Value();
}

float ctrl_float(const json::Value &element, float fallback) {
    if (!element.present() || element.is_null()) {
        return fallback;
    }
    if (element.is_number()) {
        return element.as_float().value_or(fallback);
    }
    if (element.is_string()) {
        return json::parse_java_float(element.as_string()).value_or(fallback);
    }
    if (element.is_object()) {
        float max_key_time = -1.0f;
        for (const auto &member : element.members()) {
            if (const auto time = json::parse_java_float(member.first)) {
                max_key_time = std::max(max_key_time, *time);
            }
        }
        if (max_key_time >= 0.0f) {
            return max_key_time;
        }
        for (const char *key : {"duration", "time", "value", "blend_transition"}) {
            const json::Value child = element.get(key);
            if (child.is_number()) {
                if (const auto value = child.as_float()) {
                    return *value;
                }
            }
        }
    }
    return fallback;
}

std::optional<std::string> condition_text(const std::string &value) {
    if (is_blank(value)) {
        return std::nullopt;
    }
    return value;
}

void parse_expression_list(const json::Value &element, bool merge_multiline, std::vector<std::string> &out) {
    if (!element.present()) {
        return;
    }
    auto add = [&out](const std::string &text) {
        if (!is_blank(text)) {
            out.push_back(text);
        }
    };
    if (element.is_array() && merge_multiline) {
        std::string text;
        for (const json::Value &line : element.items()) {
            if (!text.empty()) {
                text += '\n';
            }
            text += ctrl_string(line, "");
        }
        add(text);
    } else if (element.is_array()) {
        for (const json::Value &child : element.items()) {
            add(ctrl_string(child, ""));
        }
    } else {
        add(ctrl_string(element, ""));
    }
}

ControllerState parse_state(const std::string &name, const json::Value &object, bool merge_multiline) {
    ControllerState state;
    state.name = name;

    const json::Value animations = object.get("animations");
    if (animations.is_object()) {
        for (const auto &member : animations.members()) {
            state.animations.push_back({std::string(member.first), condition_text(ctrl_string(member.second, ""))});
        }
    } else if (animations.is_array()) {
        for (const json::Value &child : animations.items()) {
            if (child.is_primitive()) {
                state.animations.push_back({child.as_string(), std::nullopt});
            } else if (child.is_object()) {
                for (const auto &member : child.members()) {
                    state.animations.push_back(
                        {std::string(member.first), condition_text(ctrl_string(member.second, ""))});
                }
            }
        }
    }

    const json::Value transitions = object.get("transitions");
    if (transitions.is_object()) {
        for (const auto &member : transitions.members()) {
            state.transitions.push_back({std::string(member.first), condition_text(ctrl_string(member.second, ""))});
        }
    } else if (transitions.is_array()) {
        for (const json::Value &child : transitions.items()) {
            if (!child.is_object()) {
                continue;
            }
            for (const auto &member : child.members()) {
                state.transitions.push_back(
                    {std::string(member.first), condition_text(ctrl_string(member.second, ""))});
            }
        }
    }

    parse_expression_list(object.get("on_entry"), merge_multiline, state.on_entry);
    parse_expression_list(object.get("on_exit"), merge_multiline, state.on_exit);

    const json::Value blend = object.get("blend_transition");
    state.blend_transition = ctrl_float(blend, 0.0f);
    const json::Value shortest = object.get("blend_via_shortest_path");
    state.blend_via_shortest_path = shortest.is_primitive() && shortest.as_bool();

    // blendCurve: numeric-keyed objects are curves (YsmBlendTransition.curve); anything else is
    // YsmBlendTransition.linear(floatValue(element)). Both validate and throw like the reference.
    bool curve = false;
    std::vector<std::pair<float, float>> points;
    if (blend.is_object()) {
        curve = true;
        for (const auto &member : blend.members()) {
            const auto time = json::parse_java_float(member.first);
            const auto value = member.second.as_float();
            if (!time || !value) {
                curve = false; // NumberFormatException → linear(floatValue(element))
                points.clear();
                break;
            }
            for (const auto &point : points) {
                uint32_t a;
                uint32_t b;
                std::memcpy(&a, &point.first, 4);
                std::memcpy(&b, &*time, 4);
                if (a == b) {
                    abort_parse("IllegalArgumentException: Duplicate transition time");
                }
            }
            points.emplace_back(*time, *value);
        }
    }
    if (curve && !points.empty()) {
        // TreeMap<Float, Float> order (Float.compareTo; NaN sorts last and is rejected below).
        std::stable_sort(points.begin(), points.end(), [](const auto &a, const auto &b) {
            if (std::isnan(a.first) || std::isnan(b.first)) {
                return !std::isnan(a.first) && std::isnan(b.first);
            }
            if (a.first == b.first) {
                return std::signbit(a.first) && !std::signbit(b.first); // -0.0 before 0.0
            }
            return a.first < b.first;
        });
        for (const auto &point : points) {
            if (!std::isfinite(point.first) || point.first < 0.0f || !std::isfinite(point.second)) {
                abort_parse("IllegalArgumentException: Invalid transition point");
            }
        }
        state.blend_curve = true;
        state.blend_points = std::move(points);
    } else {
        const float duration = curve ? 0.0f : ctrl_float(blend, 0.0f);
        if (!std::isfinite(duration) || duration < 0.0f) {
            abort_parse("IllegalArgumentException: Invalid transition duration");
        }
    }

    const json::Value sounds = object.get("sound_effects");
    if (sounds.is_array()) {
        for (const json::Value &sound : sounds.items()) {
            const std::string effect =
                sound.is_object() ? ctrl_string(sound.get("effect"), "") : ctrl_string(sound, "");
            if (!is_blank(effect)) {
                state.sound_effects.push_back(effect);
            }
        }
    }
    return state;
}

Controller parse_controller(const std::string &name, const json::Value &object, bool merge_multiline) {
    Controller controller;
    controller.name = name;
    std::string initial = ctrl_string(first_of(object, {"initial_state", "initialState"}), "default");

    std::vector<ControllerState> states;
    std::unordered_map<std::string, size_t> positions;
    auto put_state = [&](const std::string &state_name, ControllerState state) {
        const auto found = positions.find(state_name);
        if (found != positions.end()) {
            states[found->second] = std::move(state);
        } else {
            positions.emplace(state_name, states.size());
            states.push_back(std::move(state));
        }
    };
    const json::Value state_element = object.get("states");
    if (state_element.is_object()) {
        for (const auto &member : state_element.members()) {
            if (member.second.is_object()) {
                const std::string state_name(member.first);
                put_state(state_name, parse_state(state_name, member.second, merge_multiline));
            }
        }
    } else if (state_element.is_array()) {
        for (const json::Value &element : state_element.items()) {
            if (!element.is_object()) {
                continue;
            }
            std::string state_name = ctrl_string(first_of(element, {"name", "id", "state"}), "");
            if (is_blank(state_name)) {
                state_name = "state_" + std::to_string(states.size());
            }
            put_state(state_name, parse_state(state_name, element, merge_multiline));
        }
    }
    if (positions.count(initial) == 0 && !states.empty()) {
        initial = states.front().name;
    }
    controller.initial_state = initial;
    controller.states = std::move(states);
    return controller;
}

} // namespace

// ---------------------------------------------------------------------------
// Public entry points
// ---------------------------------------------------------------------------
Geometry parse_geometry(const json::Value &root, const std::string &path) {
    try {
        if (!root.is_object()) {
            abort_parse("IllegalStateException: Not a JSON Object");
        }
        const json::Value geometries = array_of(root, "minecraft:geometry");
        const auto items = geometries.items();
        if (items.empty() || !items[0].is_object()) {
            abort_parse("YSM geometry does not contain minecraft:geometry");
        }
        const json::Value geometry = items[0];
        const json::Value description = json::obj(geometry, "description");

        Geometry result;
        result.path = path;
        result.texture_width = geo_int(description, "texture_width", 64);
        result.texture_height = geo_int(description, "texture_height", 64);

        std::unordered_map<std::string, size_t> positions;
        for (const json::Value &bone_json : array_of(geometry, "bones").items()) {
            if (!bone_json.is_object()) {
                continue;
            }
            const auto pivot = geo_vec3(bone_json, "pivot", 0, 0, 0);
            const auto rotation = geo_vec3(bone_json, "rotation", 0, 0, 0);
            GeometryBone bone;
            bone.name = geo_string(bone_json, "name", "unknown");
            bone.parent = geo_string(bone_json, "parent", "");
            bone.pivot = {-pivot[0], pivot[1], pivot[2]};
            bone.rotation = {-rotation[0], -rotation[1], rotation[2]};
            const bool bone_mirror = geo_bool(bone_json, "mirror", false);
            const float bone_inflate = geo_float(bone_json, "inflate", 0.0f);
            for (const json::Value &cube : array_of(bone_json, "cubes").items()) {
                if (cube.is_object()) {
                    bone.cubes.push_back(bake_cube(cube, bone_mirror, bone_inflate,
                                                   static_cast<float>(result.texture_width),
                                                   static_cast<float>(result.texture_height)));
                }
            }
            // bones.put(name, bone): a duplicate name replaces in place.
            const auto found = positions.find(bone.name);
            if (found != positions.end()) {
                result.bones[found->second] = std::move(bone);
            } else {
                positions.emplace(bone.name, result.bones.size());
                result.bones.push_back(std::move(bone));
            }
        }
        // A parent may appear later in the file. Validate after duplicate-name replacement,
        // keeping the reference behavior that an absent parent acts as a root.
        std::vector<uint8_t> parent_state(result.bones.size(), 0);
        auto parent_index = [&](size_t index) {
            const auto parent = positions.find(result.bones[index].parent);
            return parent == positions.end() ? result.bones.size() : parent->second;
        };
        for (size_t start = 0; start < result.bones.size(); ++start) {
            size_t cursor = start;
            while (cursor < result.bones.size() && parent_state[cursor] == 0) {
                parent_state[cursor] = 1;
                cursor = parent_index(cursor);
            }
            if (cursor < result.bones.size() && parent_state[cursor] == 1) {
                abort_parse("parent-cycle");
            }
            cursor = start;
            while (cursor < result.bones.size() && parent_state[cursor] == 1) {
                parent_state[cursor] = 2;
                cursor = parent_index(cursor);
            }
        }
        return result;
    } catch (const ParseAbort &abort) {
        fail(DecodeCode::invalid_data, "geometry/" + path + ": " + abort.message, 0);
    }
}

Geometry map_geometry(const binary::Geometry &raw, const std::string &path, Budget &budget) {
    auto invalid = [](const char *stage) { fail(DecodeCode::invalid_data, stage, 0); };
    auto check_vector = [&](const binary::Vec3 &values) {
        for (float value : values) {
            if (!std::isfinite(value)) {
                invalid("geometry/vector-value");
            }
        }
    };
    auto texture_dimension = [&](float value) -> int32_t {
        if (!std::isfinite(value) || value <= 0.0f || value > static_cast<float>(Budget::kMaxTexturePixels) ||
            value != std::nearbyint(value)) {
            invalid("geometry/texture-size");
        }
        return static_cast<int32_t>(value);
    };

    budget.length(static_cast<int64_t>(raw.bones.size()), 4096, "geometry/bones", 0);
    Geometry geometry;
    geometry.path = path;
    geometry.texture_width = texture_dimension(raw.texture_width);
    geometry.texture_height = texture_dimension(raw.texture_height);
    std::unordered_set<std::string> names;
    uint32_t faces = 0;
    for (const binary::Bone &bone : raw.bones) {
        budget.check("geometry/bone", 0);
        if (is_blank(bone.name) || !names.insert(bone.name).second) {
            invalid("geometry/bone-name");
        }
        check_vector(bone.pivot);
        check_vector(bone.rotation);
        GeometryBone target;
        target.name = bone.name;
        target.parent = bone.parent_name;
        target.pivot = bone.pivot;
        for (int axis = 0; axis < 3; ++axis) {
            target.rotation[axis] =
                static_cast<float>(static_cast<double>(bone.rotation[axis]) * (180.0 / 3.14159265358979323846));
            if (!std::isfinite(target.rotation[axis])) {
                invalid("geometry/rotation");
            }
        }
        for (const binary::Cube &cube : bone.cubes) {
            faces = budget.length(static_cast<int64_t>(faces) + static_cast<int64_t>(cube.faces.size()), 262144,
                                  "geometry/faces", 0);
            GeometryCube target_cube;
            target_cube.quads.reserve(cube.faces.size());
            for (const binary::Face &face : cube.faces) {
                budget.check("geometry/face", 0);
                Quad quad;
                for (int vertex = 0; vertex < 4; ++vertex) {
                    check_vector(face.positions[vertex]);
                    if (!std::isfinite(face.u[vertex]) || !std::isfinite(face.v[vertex])) {
                        invalid("geometry/uv");
                    }
                    std::copy(face.positions[vertex].begin(), face.positions[vertex].end(),
                              quad.positions.begin() + vertex * 3);
                    quad.uvs[vertex * 2] = face.u[vertex];
                    quad.uvs[vertex * 2 + 1] = face.v[vertex];
                }
                check_vector(face.normal);
                quad.normal = face.normal;
                target_cube.quads.push_back(quad);
            }
            target.cubes.push_back(std::move(target_cube));
        }
        geometry.bones.push_back(std::move(target));
    }

    // validateParents: no cycles, depth <= 128, every named parent exists.
    std::unordered_map<std::string, const GeometryBone *> by_name;
    for (const GeometryBone &bone : geometry.bones) {
        by_name.emplace(bone.name, &bone);
    }
    for (const GeometryBone &bone : geometry.bones) {
        std::unordered_set<std::string> seen;
        const GeometryBone *cursor = &bone;
        while (cursor != nullptr) {
            budget.check("geometry/parents", 0);
            if (!seen.insert(cursor->name).second) {
                invalid("geometry/parent-cycle");
            }
            budget.length(static_cast<int64_t>(seen.size()), 128, "geometry/parent-depth", 0);
            if (!is_blank(cursor->parent) && by_name.count(cursor->parent) == 0) {
                invalid("geometry/missing-parent");
            }
            const auto parent = by_name.find(cursor->parent);
            cursor = parent == by_name.end() ? nullptr : parent->second;
        }
    }
    return geometry;
}

AnimationFile parse_animations(const json::Value &root, const std::string &path, bool merge_multiline,
                               uint32_t worker_count) {
    AnimationFile file;
    file.path = path;
    try {
        if (!root.is_object()) {
            abort_parse("IllegalStateException: Not a JSON Object");
        }
        const json::Value type = root.get("ysm_animation_type");
        if (type.is_primitive()) {
            file.first_person = type.as_int() == std::optional<int32_t>(11);
        }
        if (!root.has("animations")) {
            return file;
        }
        const json::Value animations = root.get("animations");
        if (!animations.is_object()) {
            abort_parse("ClassCastException: animations is not an object");
        }
        const auto members = animations.members();

        struct ClipResult {
            std::optional<AnimationClip> clip;
            std::exception_ptr error;
        };

        std::vector<ClipResult> results(members.size());
        abi::parallel_for(members.size(), worker_count, [&](size_t index) {
            try {
                const auto &[name_view, animation] = members[index];
                const std::string name(name_view);
                if (!animation.is_object()) {
                    return;
                }
                AnimationClip clip;
                clip.name = name;
                if (animation.has("animation_length")) {
                    clip.length = require_float(animation.get("animation_length"), "animation_length");
                } else {
                    clip.length = derived_length(animation.get("bones"));
                    clip.unbounded = clip.length == std::numeric_limits<float>::max();
                }
                const json::Value loop = animation.get("loop");
                if (loop.is_primitive()) {
                    clip.loop = loop.is_bool() ? (loop.as_bool() ? LoopMode::loop : LoopMode::once)
                                               : loop_mode(loop.as_string());
                }
                const json::Value blend = animation.get("blend_weight");
                if (blend.is_primitive()) {
                    if (blend.is_number()) {
                        clip.blend_weight = require_float(blend, "blend_weight");
                    } else {
                        clip.blend_weight = 0.0f;
                        clip.blend_weight_expression = expression_text(blend.as_string());
                    }
                }
                if (animation.has("bones")) {
                    const json::Value bones = animation.get("bones");
                    if (!bones.is_object()) {
                        abort_parse("ClassCastException: bones is not an object");
                    }
                    std::unordered_map<std::string, size_t> bone_positions;
                    for (const auto &[bone_view, bone] : bones.members()) {
                        const std::string bone_name(bone_view);
                        if (!bone.is_object()) {
                            continue;
                        }
                        BoneAnimation bone_animation;
                        bone_animation.bone = bone_name;
                        bone_animation.position = parse_channel(bone.get("position"), "position");
                        bone_animation.rotation = parse_channel(bone.get("rotation"), "rotation");
                        bone_animation.scale = parse_channel(bone.get("scale"), "scale");
                        const auto found = bone_positions.find(bone_name);
                        if (found != bone_positions.end()) {
                            clip.bones[found->second] = std::move(bone_animation);
                        } else {
                            bone_positions.emplace(bone_name, clip.bones.size());
                            clip.bones.push_back(std::move(bone_animation));
                        }
                    }
                }
                parse_events(animation.get("timeline"), "timeline", clip, merge_multiline);
                parse_events(animation.get("sound_effects"), "sound", clip, false);
                parse_events(animation.get("particle_effects"), "particle", clip, false);
                std::stable_sort(clip.events.begin(), clip.events.end(),
                                 [](const AnimationEvent &a, const AnimationEvent &b) { return a.time < b.time; });
                for (size_t event_index = 0; event_index < clip.events.size(); ++event_index) {
                    clip.events[event_index].ordinal = static_cast<uint32_t>(event_index);
                }
                results[index].clip = std::move(clip);
            } catch (...) {
                results[index].error = std::current_exception();
            }
        });
        std::unordered_map<std::string, size_t> positions;
        for (auto &result : results) {
            if (result.error) {
                std::rethrow_exception(result.error);
            }
            if (!result.clip) {
                continue;
            }
            AnimationClip &clip = *result.clip;
            const auto found = positions.find(clip.name);
            if (found != positions.end()) {
                file.clips[found->second] = std::move(clip);
            } else {
                positions.emplace(clip.name, file.clips.size());
                file.clips.push_back(std::move(clip));
            }
        }
    } catch (const ParseAbort &abort) {
        // The reference swallows parse errors and keeps the clips parsed so far.
        file.diagnostics.push_back(path + ": " + abort.message);
    }
    return file;
}

ControllerFile parse_controllers(const json::Value &root, const std::string &path, bool merge_multiline) {
    ControllerFile file;
    file.path = path;
    std::unordered_map<std::string, size_t> positions;
    auto put = [&](Controller controller) {
        const auto found = positions.find(controller.name);
        if (found != positions.end()) {
            file.controllers[found->second] = std::move(controller);
        } else {
            positions.emplace(controller.name, file.controllers.size());
            file.controllers.push_back(std::move(controller));
        }
    };
    try {
        if (!root.is_object()) {
            abort_parse("IllegalStateException: Not a JSON Object");
        }
        if (!root.has("animation_controllers")) {
            return file;
        }
        const json::Value controllers = root.get("animation_controllers");
        if (controllers.is_object()) {
            for (const auto &member : controllers.members()) {
                if (member.second.is_object()) {
                    put(parse_controller(std::string(member.first), member.second, merge_multiline));
                }
            }
        } else if (controllers.is_array()) {
            for (const json::Value &element : controllers.items()) {
                if (!element.is_object()) {
                    continue;
                }
                std::string name = ctrl_string(first_of(element, {"name", "id", "controller", "animation"}), "");
                if (is_blank(name)) {
                    name = "controller_" + std::to_string(file.controllers.size());
                }
                put(parse_controller(name, element, merge_multiline));
            }
        }
    } catch (const ParseAbort &abort) {
        file.diagnostics.push_back(path + ": " + abort.message);
    }
    return file;
}

} // namespace omm::ysm
