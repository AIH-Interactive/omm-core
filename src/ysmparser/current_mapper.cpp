#include "ysmparser/current_mapper.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <unordered_set>

#include "abi/parallel.hpp"
#include "ysmparser/current_container.hpp"
#include "ysmparser/normalize.hpp"
#include "ysmparser/protobuf.hpp"
#include "yyjson.h"

namespace omm::ysm {
namespace {

using protobuf::Field;
using protobuf::Message;

[[noreturn]] void invalid(const char *stage) {
    fail(DecodeCode::invalid_data, stage, 0);
}

std::string hex_bytes(std::span<const uint8_t> bytes) {
    static constexpr char alphabet[] = "0123456789abcdef";
    std::string result;
    result.reserve(bytes.size() * 2);
    for (uint8_t byte : bytes) {
        result.push_back(alphabet[byte >> 4]);
        result.push_back(alphabet[byte & 15]);
    }
    return result;
}

std::string blob_path(uint64_t id) {
    if (id == 0 || id > std::numeric_limits<uint32_t>::max()) {
        invalid("current/blob-id");
    }
    return "blob-" + std::to_string(id);
}

binary::OrderedMap<std::span<const uint8_t>> map_entries(const Message &message, uint32_t field) {
    binary::OrderedMap<std::span<const uint8_t>> result;
    for (const Field &entry : message.all(field)) {
        const Message pair = message.message(entry);
        result.put(pair.string(1), pair.bytes(2));
    }
    return result;
}

ProgramRef read_program(const Message &message) {
    auto program = std::make_shared<Program>();
    const uint64_t format = message.integer(1);
    if (format > std::numeric_limits<uint32_t>::max()) {
        invalid("current/program-format");
    }
    program->format = static_cast<uint32_t>(format);
    const Field *payload = nullptr;
    for (const Field &field : message.fields()) {
        if (field.number == 2 || field.number == 3) {
            payload = &field;
        }
    }
    if (payload == nullptr) {
        invalid("current/program-payload");
    }
    Message::require_wire(*payload, 2);
    if (payload->number == 3) {
        program->source = std::string(reinterpret_cast<const char *>(payload->bytes.data()), payload->bytes.size());
        if (!is_valid_utf8(*program->source)) {
            invalid("current/program-source-utf8");
        }
    } else {
        program->bytecode.assign(payload->bytes.begin(), payload->bytes.end());
    }
    return program;
}

Component read_component(const Message &message) {
    Component result;
    for (const Field &field : message.fields()) {
        if (field.number == 1) {
            result.value = Message::fixed_float(field);
            result.program.reset();
            result.expression.reset();
        } else if (field.number == 2) {
            result.value = 0;
            result.program = read_program(message.message(field));
            result.expression = result.program->source;
        }
    }
    return result;
}

ComponentVector component_vector(const Message &message, uint32_t field) {
    const auto values = message.all(field);
    if (values.size() != 1 && values.size() != 3) {
        invalid("current/channel-vector-size");
    }
    ComponentVector result;
    for (size_t index = 0; index < 3; ++index) {
        result[index] = read_component(message.message(values[values.size() == 1 ? 0 : index]));
    }
    return result;
}

std::optional<Channel> read_channel(const Message &bone, uint32_t field, bool scale) {
    const auto frames = bone.all(field);
    if (frames.empty()) {
        return std::nullopt;
    }
    Channel channel;
    channel.keyframed = true;
    for (Component &component : channel.constant) {
        component.value = scale ? 1.0f : 0.0f;
    }
    for (const Field &raw : frames) {
        const Message frame = bone.message(raw);
        Keyframe keyframe;
        keyframe.time = frame.number(1) / 20.0f;
        switch (frame.integer(2)) {
        case 0:
            keyframe.interpolation = "step";
            break;
        case 1:
            keyframe.interpolation = "linear";
            break;
        case 2:
            keyframe.interpolation = "catmullrom";
            break;
        default:
            invalid("current/easing");
        }
        keyframe.pre = component_vector(frame, 3);
        keyframe.post = frame.last(4) == nullptr ? keyframe.pre : component_vector(frame, 4);
        channel.keyframes.push_back(std::move(keyframe));
    }
    std::stable_sort(channel.keyframes.begin(), channel.keyframes.end(),
                     [](const Keyframe &a, const Keyframe &b) { return a.time < b.time; });
    for (size_t index = 0; index < channel.keyframes.size(); ++index) {
        channel.keyframes[index].ordinal = static_cast<uint32_t>(index);
    }
    return channel;
}

AnimationFile read_animations(const Message &raw, const std::string &path, bool first_person, uint32_t worker_count) {
    AnimationFile result;
    result.path = path;
    result.first_person = first_person;
    std::unordered_set<std::string> names;
    const auto entries = raw.all(1);
    for (const Field &entry : entries) {
        const std::string name = raw.message(entry).string(1);
        if (name.empty() || !names.insert(name).second) {
            invalid("current/animation-name");
        }
    }
    std::vector<AnimationClip> clips(entries.size());
    abi::parallel_for(entries.size(), worker_count, [&](size_t index) {
        const Message animation = raw.message(entries[index]);
        AnimationClip clip;
        clip.name = animation.string(1);
        const float ticks = animation.number(2);
        clip.unbounded = ticks == std::numeric_limits<float>::max();
        clip.length = clip.unbounded ? ticks : ticks / 20.0f;
        switch (animation.integer(3)) {
        case 0:
        case 2:
            clip.loop = LoopMode::once;
            break;
        case 1:
            clip.loop = LoopMode::loop;
            break;
        case 3:
            clip.loop = LoopMode::hold_on_last_frame;
            break;
        default:
            invalid("current/loop");
        }
        if (animation.last(4) != nullptr) {
            const Component blend = read_component(animation.message(4));
            clip.blend_weight = blend.value;
            clip.blend_weight_expression = blend.expression;
            clip.blend_weight_program = blend.program;
        }
        std::unordered_set<std::string> bones;
        for (const Field &bone_field : animation.all(5)) {
            const Message bone = animation.message(bone_field);
            BoneAnimation mapped;
            mapped.bone = bone.string(1);
            if (mapped.bone.empty() || !bones.insert(mapped.bone).second) {
                invalid("current/animation-bone-name");
            }
            mapped.rotation = read_channel(bone, 2, false);
            mapped.position = read_channel(bone, 3, false);
            mapped.scale = read_channel(bone, 4, true);
            clip.bones.push_back(std::move(mapped));
        }
        for (const Field &event_field : animation.all(6)) {
            const Message event = animation.message(event_field);
            AnimationEvent mapped;
            mapped.type = "timeline";
            mapped.time = event.number(2) / 20.0f;
            mapped.program = read_program(event.message(1));
            mapped.text = mapped.program->source.value_or("");
            clip.events.push_back(std::move(mapped));
        }
        for (const Field &event_field : animation.all(7)) {
            const Message event = animation.message(event_field);
            AnimationEvent mapped;
            mapped.type = "sound";
            mapped.time = event.number(2) / 20.0f;
            mapped.text = event.string(1);
            clip.events.push_back(std::move(mapped));
        }
        std::stable_sort(clip.events.begin(), clip.events.end(),
                         [](const AnimationEvent &a, const AnimationEvent &b) { return a.time < b.time; });
        for (size_t event_index = 0; event_index < clip.events.size(); ++event_index) {
            clip.events[event_index].ordinal = static_cast<uint32_t>(event_index);
        }
        clips[index] = std::move(clip);
    });
    result.clips = std::move(clips);
    return result;
}

ControllerFile read_controllers(const Message &raw, const std::string &path) {
    ControllerFile file;
    file.path = path;
    std::unordered_set<std::string> names;
    for (const Field &entry : raw.all(1)) {
        const Message controller = raw.message(entry);
        Controller mapped;
        mapped.name = controller.string(1);
        mapped.initial_state = controller.string(2, "default");
        if (mapped.name.empty() || !names.insert(mapped.name).second) {
            invalid("current/controller-name");
        }
        std::unordered_set<std::string> states;
        for (const Field &state_field : controller.all(3)) {
            const Message state = controller.message(state_field);
            ControllerState target;
            target.name = state.string(1);
            if (target.name.empty() || !states.insert(target.name).second) {
                invalid("current/controller-state-name");
            }
            for (const Field &reference : state.all(2)) {
                const Message value = state.message(reference);
                ControllerAnimationRef animation;
                animation.animation = value.string(1);
                if (value.last(2) != nullptr) {
                    animation.program = read_program(value.message(2));
                    animation.weight = animation.program->source;
                }
                target.animations.push_back(std::move(animation));
            }
            for (const Field &reference : state.all(3)) {
                const Message value = state.message(reference);
                ControllerTransition transition;
                transition.target = value.string(1);
                transition.program = read_program(value.message(2));
                transition.condition = transition.program->source;
                target.transitions.push_back(std::move(transition));
            }
            if (state.last(4) != nullptr) {
                target.entry_program = read_program(state.message(4));
            }
            if (state.last(5) != nullptr) {
                target.exit_program = read_program(state.message(5));
            }
            if (state.last(6) != nullptr) {
                const Message blend = state.message(6);
                target.blend_transition = blend.number(1);
                target.blend_curve = blend.last(2) != nullptr;
                for (const Field &point_field : blend.all(2)) {
                    const Message point = blend.message(point_field);
                    target.blend_points.emplace_back(point.number(1), point.number(2));
                }
                std::stable_sort(target.blend_points.begin(), target.blend_points.end());
            }
            for (const Field &sound : state.all(8)) {
                Message::require_wire(sound, 2);
                target.sound_effects.emplace_back(reinterpret_cast<const char *>(sound.bytes.data()),
                                                  sound.bytes.size());
            }
            mapped.states.push_back(std::move(target));
        }
        file.controllers.push_back(std::move(mapped));
    }
    return file;
}

Geometry read_geometry(std::span<const uint8_t> bytes, const std::string &path, Budget &budget) {
    const Message raw(bytes);
    binary::Geometry geometry;
    const Message properties = raw.message(2);
    geometry.texture_height = properties.number(1, 64);
    geometry.texture_width = properties.number(2, 64);
    const Message cubes(raw.bytes(3));
    const auto cube_fields = cubes.all(1);
    size_t next_cube = 0;
    const auto bone_fields = raw.all(1);
    budget.length(static_cast<int64_t>(bone_fields.size()), 16384, "current/bones", 0);
    for (const Field &bone_field : bone_fields) {
        const Message bone = raw.message(bone_field);
        binary::Bone mapped;
        mapped.name = bone.string(1);
        mapped.parent_name = bone.string(2);
        const auto pivot = bone.floats(3);
        const auto rotation = bone.floats(4);
        if (pivot.size() != 3 || rotation.size() != 3) {
            invalid("current/bone-vector-size");
        }
        std::copy(pivot.begin(), pivot.end(), mapped.pivot.begin());
        std::copy(rotation.begin(), rotation.end(), mapped.rotation.begin());
        const uint64_t count = bone.integer(6);
        if (count > cube_fields.size() - next_cube) {
            invalid("current/bone-cube-count");
        }
        for (uint64_t index = 0; index < count; ++index) {
            const Message cube = cubes.message(cube_fields[next_cube++]);
            const uint64_t faces = cube.integer(1);
            if (faces > 6) {
                invalid("current/cube-face-count");
            }
            const auto positions = cube.floats(2);
            const auto position_indices = cube.integers(3);
            const auto uvs = cube.floats(4);
            const auto uv_indices = cube.integers(5);
            const auto normals = cube.floats(6);
            if (positions.size() % 3 != 0 || uvs.size() % 2 != 0 || position_indices.size() != faces * 4 ||
                uv_indices.size() != faces * 4 || normals.size() != faces * 3) {
                invalid("current/cube-array-size");
            }
            binary::Cube target_cube;
            for (size_t face_index = 0; face_index < faces; ++face_index) {
                binary::Face face;
                std::copy_n(normals.begin() + face_index * 3, 3, face.normal.begin());
                for (size_t vertex = 0; vertex < 4; ++vertex) {
                    const uint64_t position = position_indices[face_index * 4 + vertex];
                    const uint64_t uv = uv_indices[face_index * 4 + vertex];
                    if (position >= positions.size() / 3 || uv >= uvs.size() / 2) {
                        invalid("current/cube-index");
                    }
                    std::copy_n(positions.begin() + static_cast<size_t>(position) * 3, 3,
                                face.positions[vertex].begin());
                    face.u[vertex] = uvs[static_cast<size_t>(uv) * 2];
                    face.v[vertex] = uvs[static_cast<size_t>(uv) * 2 + 1];
                }
                target_cube.faces.push_back(face);
            }
            mapped.cubes.push_back(std::move(target_cube));
        }
        geometry.bones.push_back(std::move(mapped));
    }
    if (next_cube != cube_fields.size()) {
        invalid("current/unclaimed-cubes");
    }
    return map_geometry(geometry, path, budget);
}

class Mapper {
public:
    Mapper(CurrentContainer container, Budget &budget, const std::string &display_name)
        : container_(std::move(container)), budget_(budget), asset_(std::make_unique<Asset>()) {
        asset_->source.container = SourceContainer::current;
        asset_->source.binary_format = 1;
        asset_->manifest.kind = AvatarKind::current;
        asset_->manifest.name = display_name;
    }

    std::unique_ptr<Asset> read() {
        if (container_.schema != "mixel/character" || container_.properties[0] != "0.1.1-unstable") {
            fail(DecodeCode::unsupported_version, "current/model-schema", 0);
        }
        const Message manifest(resource("manifest").bytes);
        const Message info = manifest.message(3);
        const Message properties = info.message(2);
        const auto model_id = properties.bytes(1);
        if (model_id.size() != 32 || hex_bytes(model_id) != container_.properties[2]) {
            invalid("current/model-identity");
        }
        const Message metadata = info.message(4);
        asset_->manifest.name = metadata.string(1, asset_->manifest.name);
        asset_->manifest.description = metadata.string(2);
        asset_->manifest.free = properties.integer(2) != 0;
        for (const Field &author_field : metadata.all(4)) {
            const Message author = metadata.message(author_field);
            asset_->manifest.authors.push_back(author.string(1));
            if (author.last(5) != nullptr) {
                image(author.message(5));
            }
        }
        const Message settings = info.message(3);
        asset_->manifest.default_texture = settings.string(4);
        if (settings.last(7) != nullptr) {
            asset_->manifest.gui_foreground = image(settings.message(7));
        }
        if (settings.last(8) != nullptr) {
            asset_->manifest.gui_background = image(settings.message(8));
        }
        const bool icon = container_.chunks.exists("thumb-icon");
        const bool thumbnail = container_.chunks.exists("thumb-button");
        if ((info.integer(7) != 0) != icon || (info.integer(6) != 0) != thumbnail || info.integer(7) > 2 ||
            info.integer(6) > 2) {
            invalid("current/preview-source");
        }
        if (icon) {
            asset_->manifest.avatar_image = "thumb-icon";
        }
        if (thumbnail) {
            asset_->manifest.thumbnail_image = "thumb-button";
        }
        const Message common = manifest.message(2);
        if (common.integer(2) != 0) {
            const std::string path = blob_path(common.integer(2));
            const Message strings(resource(path).bytes);
            role(path, "strings");
            for (const Field &function_field : strings.all(1)) {
                const Message function = strings.message(function_field);
                asset_->programs.push_back({function.string(1), read_program(function.message(2))});
            }
        }
        for (const Field &sound_field : common.all(1)) {
            const Message sound = common.message(sound_field);
            for (uint64_t id : sound.integers(7)) {
                const std::string path = blob_path(id);
                resource(path);
                role(path, "sound");
                asset_->manifest.sounds.push_back(path);
            }
        }
        std::unordered_set<std::string> targets;
        bool player = false;
        for (const Field &target_field : manifest.all(1)) {
            const Message target = manifest.message(target_field);
            const std::string id = target.string(1);
            const uint64_t kind = target.integer(2);
            const auto normalized = normalize_relative(id);
            if (!normalized || *normalized != id || id.empty() || !targets.insert(id).second ||
                (kind != 1 && kind != 3 && kind != 4) || (id == "player") != (kind == 1)) {
                invalid("current/render-target");
            }
            player = player || kind == 1;
            target_data(target, id, kind);
        }
        if (!player || asset_->manifest.main_model.empty() || asset_->manifest.textures.empty()) {
            invalid("current/missing-player-data");
        }
        asset_->metadata_json = source_metadata(model_id);
        asset_->resources = std::move(container_.chunks);
        for (const Resource &entry : asset_->resources.entries()) {
            const auto found = images_.find(entry.path);
            ResourceInfo resource_info;
            if (found != images_.end()) {
                resource_info = found->second;
            } else {
                resource_info.media_type = "application/octet-stream";
            }
            asset_->resource_info.push_back(std::move(resource_info));
        }
        return std::move(asset_);
    }

private:
    const Resource &resource(const std::string &path) {
        const Resource *value = container_.chunks.find(path);
        if (value == nullptr) {
            invalid("current/missing-blob");
        }
        return *value;
    }

    void role(const std::string &path, const std::string &value) {
        const auto [found, inserted] = roles_.emplace(path, value);
        if (!inserted && found->second != value) {
            invalid("current/conflicting-blob-role");
        }
    }

    std::string image(const Message &value) {
        const std::string path = blob_path(value.integer(1));
        resource(path);
        role(path, "image");
        const std::string format = lower_ascii(value.string(2));
        if (format.empty() || value.integer(3) > 16384 || value.integer(4) > 16384 ||
            value.integer(3) * value.integer(4) > Budget::kMaxTexturePixels) {
            invalid("current/image-metadata");
        }
        ResourceInfo info;
        info.media_type = "image/" + format;
        info.width = static_cast<int32_t>(value.integer(3));
        info.height = static_cast<int32_t>(value.integer(4));
        images_.emplace(path, std::move(info));
        return path;
    }

    std::string derived_resource(const std::string &prefix, std::span<const uint8_t> bytes) {
        const std::string path = "current/" + prefix + "/" + std::to_string(derived_id_++) + ".pb";
        budget_.begin_resource("current/derived-count", 0);
        budget_.resource(bytes.size(), "current/derived-bytes", 0);
        container_.chunks.add(path, std::vector<uint8_t>(bytes.begin(), bytes.end()), "current/derived-path");
        return path;
    }

    void target_data(const Message &target, const std::string &id, uint64_t kind) {
        const std::string path = blob_path(target.integer(4));
        role(path, "definition");
        const Message definition(resource(path).bytes);
        if (kind == 3) {
            asset_->manifest.projectiles.push_back(path);
        } else if (kind == 4) {
            asset_->manifest.vehicles.push_back(path);
        }
        const auto geometries = map_entries(definition, 1);
        std::vector<std::string> geometry_paths;
        for (const auto &[key, data] : geometries.entries()) {
            const std::string geometry_path = derived_resource(id + "/geometry", data);
            geometry_paths.push_back(geometry_path);
            if (kind == 1 && key == "main") {
                asset_->manifest.main_model = geometry_path;
            } else if (kind == 1 && key == "arm") {
                asset_->manifest.arm_model = geometry_path;
            }
        }
        std::vector<Geometry> geometry_results(geometry_paths.size());
        abi::parallel_for(geometry_results.size(), budget_.limits().worker_count, [&](size_t index) {
            Budget local_budget(budget_.limits());
            geometry_results[index] =
                read_geometry(geometries.entries()[index].second, geometry_paths[index], local_budget);
        });
        for (Geometry &geometry : geometry_results) {
            asset_->geometries.push_back(std::move(geometry));
        }
        const auto animations = map_entries(definition, 2);
        std::vector<std::string> animation_paths;
        for (const auto &[key, data] : animations.entries()) {
            const std::string animation_path = derived_resource(id + "/animation", data);
            asset_->manifest.animations.push_back(animation_path);
            animation_paths.push_back(animation_path);
        }
        std::vector<AnimationFile> animation_results(animation_paths.size());
        const size_t participants =
            std::max<size_t>(1, std::min<size_t>(budget_.limits().worker_count, animation_results.size()));
        const uint32_t clip_threads = std::max(1u, budget_.limits().worker_count / static_cast<uint32_t>(participants));
        abi::parallel_for(animation_results.size(), budget_.limits().worker_count, [&](size_t index) {
            const auto &[key, data] = animations.entries()[index];
            animation_results[index] =
                read_animations(Message(data), animation_paths[index], key == "fp_arm", clip_threads);
        });
        for (AnimationFile &animation : animation_results) {
            asset_->animation_files.push_back(std::move(animation));
        }
        const auto controllers = map_entries(definition, 3);
        std::vector<std::string> controller_paths;
        for (const auto &[key, data] : controllers.entries()) {
            const std::string controller_path = derived_resource(id + "/controller", data);
            asset_->manifest.controllers.push_back(controller_path);
            controller_paths.push_back(controller_path);
        }
        std::vector<ControllerFile> controller_results(controller_paths.size());
        abi::parallel_for(controller_results.size(), budget_.limits().worker_count, [&](size_t index) {
            controller_results[index] =
                read_controllers(Message(controllers.entries()[index].second), controller_paths[index]);
        });
        for (ControllerFile &controller : controller_results) {
            asset_->controller_files.push_back(std::move(controller));
        }
        const auto textures = map_entries(target, 5);
        for (const auto &[key, data] : textures.entries()) {
            const Message texture(data);
            TextureOption mapped;
            mapped.id = key;
            mapped.name = key;
            mapped.path = image(texture.message(1));
            if (texture.last(2) != nullptr) {
                mapped.normal_path = image(texture.message(2));
            }
            if (texture.last(3) != nullptr) {
                mapped.specular_path = image(texture.message(3));
            }
            if (kind == 1) {
                asset_->manifest.textures.push_back(std::move(mapped));
            }
        }
    }

    std::string source_metadata(std::span<const uint8_t> model_id) {
        std::unique_ptr<yyjson_mut_doc, decltype(&yyjson_mut_doc_free)> doc(yyjson_mut_doc_new(nullptr),
                                                                            yyjson_mut_doc_free);
        if (!doc) {
            throw std::bad_alloc();
        }
        yyjson_mut_val *root = yyjson_mut_obj(doc.get());
        yyjson_mut_doc_set_root(doc.get(), root);
        auto put = [&](const char *key, const std::string &value) {
            yyjson_mut_obj_add_strcpy(doc.get(), root, key, value.c_str());
        };
        put("schema", container_.schema);
        put("schema_version", container_.properties[0]);
        put("model_id", hex_bytes(model_id));
        put("container_id", hex_bytes(container_.container_id));
        put("manifest_resource", "manifest");
        size_t length = 0;
        char *text = yyjson_mut_write(doc.get(), 0, &length);
        if (text == nullptr) {
            throw std::bad_alloc();
        }
        std::string result(text, length);
        std::free(text);
        return result;
    }

    CurrentContainer container_;
    Budget &budget_;
    std::unique_ptr<Asset> asset_;
    std::map<std::string, std::string> roles_;
    std::map<std::string, ResourceInfo> images_;
    uint32_t derived_id_ = 0;
};

} // namespace

std::unique_ptr<Asset> parse_current_asset(std::span<const uint8_t> input, const std::string &display_name,
                                           const Limits &limits) {
    Budget budget(limits);
    Mapper mapper(decode_current_container(input, budget), budget, display_name);
    return mapper.read();
}

} // namespace omm::ysm
