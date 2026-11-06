// In-memory form of the YSM binary model payload (crypto v3, format 1..32).
// Field-for-field port of OMP YsmBinaryModel; unknown fields are kept so that
// the mapping layer can expose them exactly as the reference does.
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "ysmparser/decode.hpp"

namespace omm::ysm::binary {

// Insertion-ordered string map with LinkedHashMap semantics: put() on an existing key
// replaces the value but keeps its original position.
template <typename Value>
class OrderedMap {
public:
    using Entry = std::pair<std::string, Value>;

    Value &put(const std::string &key, Value value) {
        const auto found = index_.find(key);
        if (found != index_.end()) {
            entries_[found->second].second = std::move(value);
            return entries_[found->second].second;
        }
        index_.emplace(key, entries_.size());
        entries_.emplace_back(key, std::move(value));
        return entries_.back().second;
    }

    bool contains(const std::string &key) const {
        return index_.count(key) != 0;
    }

    Value *find(const std::string &key) {
        const auto found = index_.find(key);
        return found == index_.end() ? nullptr : &entries_[found->second].second;
    }

    const Value *find(const std::string &key) const {
        const auto found = index_.find(key);
        return found == index_.end() ? nullptr : &entries_[found->second].second;
    }

    bool erase(const std::string &key) {
        const auto found = index_.find(key);
        if (found == index_.end()) {
            return false;
        }
        entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(found->second));
        index_.clear();
        for (size_t position = 0; position < entries_.size(); ++position) {
            index_.emplace(entries_[position].first, position);
        }
        return true;
    }

    std::vector<Entry> &entries() noexcept {
        return entries_;
    }

    const std::vector<Entry> &entries() const noexcept {
        return entries_;
    }

    size_t size() const noexcept {
        return entries_.size();
    }

    bool empty() const noexcept {
        return entries_.empty();
    }

private:
    std::vector<Entry> entries_;
    std::unordered_map<std::string, size_t> index_;
};

// Keyframe/blend values are either a float or a Molang string.
using Value = std::variant<float, std::string>;

using Vec3 = std::array<float, 3>;

struct Face {
    Vec3 normal{};
    std::array<Vec3, 4> positions{};
    std::array<float, 4> u{};
    std::array<float, 4> v{};
};

struct Cube {
    std::vector<Face> faces;
    int32_t unk_int1 = 0;
    int32_t unk_int2 = 0;
    int32_t unk_int3 = 0;
};

struct Bone {
    std::string name;
    std::string parent_name;
    Vec3 pivot{};
    Vec3 rotation{};
    std::array<int32_t, 5> unk_pad{};
    std::vector<Cube> cubes;
};

struct Geometry {
    int32_t model_type = 0; // 1 = main, 2 = arm, 3 = arrow
    std::string identifier;
    std::string sha256;
    float texture_width = 64.0f;
    float texture_height = 64.0f;
    float visible_bounds_width = 0.0f;
    float visible_bounds_height = 0.0f;
    std::vector<float> visible_bounds_offset;
    float unk_float1 = 0.0f;
    float unk_float2 = 0.0f;
    std::array<int32_t, 3> footer_pad{};
    std::vector<Bone> bones;
};

struct Keyframe {
    float timestamp = 0.0f;
    int32_t interpolation_mode = 0; // 0 linear, 1 step, 2 catmullrom, 3 bezier
    std::array<Value, 3> post_data{};
    std::array<Value, 3> pre_data{};
    bool has_pre_data = false;
};

struct BoneAnimation {
    std::string bone_name;
    std::vector<Keyframe> rotation;
    std::vector<Keyframe> position;
    std::vector<Keyframe> scale;
};

struct TimelineEvent {
    float timestamp = 0.0f;
    std::vector<std::string> events;
};

struct SoundEffect {
    std::string effect_name;
    float timestamp = 0.0f;
};

struct Animation {
    std::string name;
    float length = 0.0f; // seconds; +infinity means animation_length was omitted
    int32_t loop_mode = 0;
    std::optional<Value> blend_weight;
    int32_t unk_int1 = 0;
    int32_t unk_int2 = 0;
    int32_t unk_int4 = 0;
    std::vector<BoneAnimation> bone_animations;
    std::vector<TimelineEvent> timeline_events;
    std::vector<SoundEffect> sound_effects;
};

struct AnimationFile {
    int32_t anim_type = 0;
    std::optional<std::string> file_hash; // null until a legacy hash table sets it
    OrderedMap<Animation> animations;
};

struct ControllerState {
    std::string name;
    OrderedMap<std::string> animations;
    OrderedMap<std::string> transitions;
    std::vector<std::string> on_entry;
    std::vector<std::string> on_exit;
    std::vector<std::string> sound_effects;
    float blend_transition_value = 0.0f;
    bool blend_via_shortest_path = false;
    // Map<Float, Float> with LinkedHashMap semantics.
    std::vector<std::pair<float, float>> blend_transitions;
};

struct Controller {
    std::string animation_name;
    std::string initial_state;
    std::vector<ControllerState> states;
};

struct ControllerFile {
    std::optional<std::string> name; // null when the format carries no name
    std::optional<std::string> hash;
    int32_t legacy_unknown_int = 0;
    OrderedMap<Controller> controllers;
};

struct SubTexture {
    std::optional<std::string> hash;
    int32_t specular_type = 0;
    int32_t width = 0;
    int32_t height = 0;
    int32_t image_format = 0;
    std::vector<uint8_t> data;
    int32_t unknown_flag = 0;
};

struct Texture {
    std::string name;
    std::optional<std::string> hash;
    int32_t width = 0;
    int32_t height = 0;
    int32_t image_format = 0; // -1 = raw RGBA
    std::vector<uint8_t> data;
    int32_t unknown_flag = 0;
    std::vector<SubTexture> sub_textures;
};

struct Image {
    std::string name;
    std::vector<uint8_t> data;
    int32_t width = 0;
    int32_t height = 0;
    int32_t format = 0;
    int32_t unknown_flag = 0;
};

struct Author {
    std::string name;
    std::string role;
    std::string comment;
    OrderedMap<std::string> contacts;
    std::string avatar;
    std::optional<size_t> avatar_image; // index into Model::images
};

struct Metadata {
    std::string name;
    std::string tips;
    std::string license_type;
    std::string license_description;
    std::vector<Author> authors;
    OrderedMap<std::string> links;
    std::vector<size_t> extra_avatars; // indices into Model::images
};

struct ConfigForm {
    std::string type;
    std::string title;
    std::string description;
    std::string default_value;
    float step = 0.0f;
    float min = 0.0f;
    float max = 0.0f;
    OrderedMap<std::string> labels;
};

struct ExtraAnimationButton {
    std::string id;
    std::string name;
    std::string description;
    std::vector<ConfigForm> forms;
};

struct ExtraAnimationClassify {
    std::string id;
    OrderedMap<std::string> extras;
};

struct Properties {
    std::string sha256;
    float width_scale = 0.7f;
    float height_scale = 0.7f;
    std::string default_texture = "default";
    std::string preview_animation;
    bool is_free = false;
    bool render_layers_first = false;
    bool all_cutout = false;
    bool disable_preview_rotation = false;
    bool gui_no_lighting = false;
    bool merge_multiline_expr = false;
    std::string gui_foreground;
    std::string gui_background;
    std::vector<size_t> background_images; // indices into Model::images
    OrderedMap<std::string> extra_animations;
    std::vector<ExtraAnimationClassify> extra_animation_classifies;
    std::vector<ExtraAnimationButton> extra_animation_buttons;
};

struct SubEntity {
    std::string identifier;
    std::vector<std::string> match_ids; // empty means "not set"
    std::optional<Geometry> model;
    OrderedMap<Texture> textures;
    OrderedMap<AnimationFile> animation_files;
    std::vector<ControllerFile> animation_controller_files;
};

struct MainEntity {
    std::optional<Geometry> main_model;
    std::optional<Geometry> arm_model;
    OrderedMap<Texture> textures;
    OrderedMap<AnimationFile> animation_files;
    std::vector<ControllerFile> animation_controller_files;
};

struct DataFile {
    std::string hash;
    std::vector<uint8_t> data;
};

struct LanguageFile {
    std::string hash;
    OrderedMap<std::string> data;
};

struct Footer {
    int32_t version = 65535;
    int32_t unk_int1 = 1;
    std::string rand;
    int64_t time = 0;
    std::string extra;
    int32_t unk_int2 = 0;
};

struct Model {
    int32_t format_version = 0;
    Metadata metadata;
    Properties properties;
    MainEntity main_entity;
    OrderedMap<SubEntity> vehicles;
    OrderedMap<SubEntity> projectiles;
    OrderedMap<DataFile> sound_files;
    OrderedMap<DataFile> function_files;
    OrderedMap<LanguageFile> language_files;
    Footer footer;
    // Owns every author/extra/background image; referenced by index.
    std::vector<Image> images;
};

// Parses a decrypted v3 payload (leading format dword). Throws on any structural error.
Model read_payload(std::span<const uint8_t> payload, Budget &budget);

} // namespace omm::ysm::binary
