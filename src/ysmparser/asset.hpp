// Normalized, immutable YSM asset produced by the native ysmparser.
//
// One representation for every source (raw directory, ZIP, legacy YSGP v1/v2, v3 binary):
// geometry is baked quads (as OMP YsmGeometry), animations and controllers are the data
// half of OMP's YsmAnimationParser / YsmAnimationControllerParser (expressions are kept as
// text; Molang compilation stays with the host), and resources stay addressable by path.
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ysmparser/resource_set.hpp"

namespace omm::ysm {

// ---- Geometry ----

struct Quad {
    std::array<float, 12> positions{}; // 4 vertices, model units (1/16 block scaled)
    std::array<float, 8> uvs{};        // normalized
    std::array<float, 3> normal{};
};

struct GeometryCube {
    std::vector<Quad> quads;
};

struct GeometryBone {
    std::string name;
    std::string parent;
    std::array<float, 3> pivot{};
    std::array<float, 3> rotation{}; // degrees
    std::vector<GeometryCube> cubes;
};

struct Geometry {
    std::string path; // resource path the geometry came from
    int32_t texture_width = 64;
    int32_t texture_height = 64;
    std::vector<GeometryBone> bones; // LinkedHashMap order; roots/children derive from parent
};

// ---- Animation ----

// Current-schema expressions remain host-owned portable programs; native never evaluates them.
struct Program {
    uint32_t format = 0;
    std::optional<std::string> source;
    std::vector<uint8_t> bytecode;
};

using ProgramRef = std::shared_ptr<const Program>;

struct NamedProgram {
    std::string name;
    ProgramRef program;
};

// One vector component: a constant, or a Molang expression with the constant fallback.
struct Component {
    float value = 0.0f;
    std::optional<std::string> expression;
    ProgramRef program;
};

using ComponentVector = std::array<Component, 3>;

struct Keyframe {
    float time = 0.0f;
    std::string interpolation = "linear";
    ComponentVector post;
    ComponentVector pre;
    uint32_t ordinal = 0; // position after the stable time sort
};

struct Channel {
    ComponentVector constant; // value used when there are no keyframes
    bool keyframed = false;
    std::vector<Keyframe> keyframes;
};

struct BoneAnimation {
    std::string bone;
    std::optional<Channel> position;
    std::optional<Channel> rotation;
    std::optional<Channel> scale;
};

struct AnimationEvent {
    float time = 0.0f;
    std::string type; // timeline | sound | particle
    std::string text;
    std::vector<std::pair<std::string, std::string>> params;
    uint32_t ordinal = 0;
    ProgramRef program;
};

enum class LoopMode { once, loop, hold_on_last_frame };

struct AnimationClip {
    std::string name;
    float length = 0.0f;
    bool unbounded = false; // derived length with no keyframes (Float.MAX_VALUE in OMP)
    LoopMode loop = LoopMode::once;
    float blend_weight = 1.0f;
    std::optional<std::string> blend_weight_expression;
    ProgramRef blend_weight_program;
    std::vector<BoneAnimation> bones;
    std::vector<AnimationEvent> events;
};

struct AnimationFile {
    std::string path;
    bool first_person = false;
    std::vector<AnimationClip> clips;
    std::vector<std::string> diagnostics;
};

// ---- Controllers ----

struct ControllerAnimationRef {
    std::string animation;
    std::optional<std::string> weight; // null = always active
    ProgramRef program;
};

struct ControllerTransition {
    std::string target;
    std::optional<std::string> condition;
    ProgramRef program;
};

struct ControllerState {
    std::string name;
    std::vector<ControllerAnimationRef> animations;
    std::vector<ControllerTransition> transitions;
    std::vector<std::string> on_entry;
    std::vector<std::string> on_exit;
    ProgramRef entry_program;
    ProgramRef exit_program;
    float blend_transition = 0.0f;
    bool blend_via_shortest_path = false;
    bool blend_curve = false;
    std::vector<std::pair<float, float>> blend_points;
    std::vector<std::string> sound_effects;
};

struct Controller {
    std::string name;
    std::string initial_state;
    std::vector<ControllerState> states;
};

struct ControllerFile {
    std::string path;
    std::vector<Controller> controllers;
    std::vector<std::string> diagnostics;
};

// ---- Manifest ----

enum class AvatarKind { current, legacy };

struct TextureOption {
    std::string id;
    std::string name;
    std::string path;
    std::string normal_path;
    std::string specular_path;
};

struct Manifest {
    AvatarKind kind = AvatarKind::current;
    std::string name;
    std::string description;
    std::vector<std::string> authors;
    std::string main_model;
    std::string arm_model;
    std::vector<std::string> animations;
    std::vector<std::string> controllers;
    std::vector<std::string> first_person_animations;
    std::vector<TextureOption> textures;
    std::string default_texture;
    bool free = false;
    bool merge_multiline_expressions = false;
    std::vector<std::string> functions;
    std::vector<std::string> sounds;
    std::vector<std::string> languages;
    std::vector<std::string> projectiles;
    std::vector<std::string> vehicles;
    std::string avatar_image;
    std::string background_image;
    std::string gui_foreground;
    std::string gui_background;
    std::string thumbnail_image;
    std::vector<std::string> warnings;
};

// ---- Source identity and the asset ----

enum class SourceContainer { directory, zip, legacy_v1, legacy_v2, v3, current };

struct SourceInfo {
    SourceContainer container = SourceContainer::directory;
    int32_t binary_format = 0; // v3 payload format (1..32), 0 otherwise
    std::string zip_root;      // directory chosen as model root inside a ZIP
};

struct ResourceInfo {
    std::string media_type;
    int32_t width = 0; // raw RGBA images only
    int32_t height = 0;
};

struct Asset {
    SourceInfo source;
    Manifest manifest;
    std::string metadata_json; // compact JSON of the source manifest root (ysm.json or mapped)
    std::vector<Geometry> geometries;
    std::vector<AnimationFile> animation_files;
    std::vector<ControllerFile> controller_files;
    ResourceSet resources;
    std::vector<ResourceInfo> resource_info; // parallel to resources.entries()
    std::vector<std::string> diagnostics;
    uint64_t retained_bytes = 0;
    std::vector<NamedProgram> programs;
};

} // namespace omm::ysm
