#include "ysmparser/parse.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>

#include "abi/parallel.hpp"
#include "ysmparser/binary_mapper.hpp"
#include "ysmparser/current_container.hpp"
#include "ysmparser/current_mapper.hpp"
#include "ysmparser/manifest.hpp"
#include "ysmparser/normalize.hpp"
#include "ysmparser/ysgp.hpp"
#include "yyjson.h"

namespace omm::ysm {

namespace {

bool starts_with_bytes(std::span<const uint8_t> data, std::initializer_list<uint8_t> prefix, size_t offset = 0) {
    if (data.size() < offset + prefix.size()) {
        return false;
    }
    size_t index = offset;
    for (uint8_t value : prefix) {
        if (data[index++] != value) {
            return false;
        }
    }
    return true;
}

bool is_zip(std::span<const uint8_t> input) noexcept {
    return starts_with_bytes(input, {0x50, 0x4B, 0x03, 0x04}) || starts_with_bytes(input, {0x50, 0x4B, 0x05, 0x06});
}

std::string sniff_media_type(const std::vector<uint8_t> &bytes, const std::string &path) {
    const std::string lower = lower_ascii(path);
    auto ends = [&lower](std::string_view suffix) {
        return lower.size() >= suffix.size() && lower.compare(lower.size() - suffix.size(), suffix.size(), suffix) == 0;
    };
    if (starts_with_bytes(bytes, {0x89, 'P', 'N', 'G'}))
        return "image/png";
    if (starts_with_bytes(bytes, {0xFF, 0xD8, 0xFF}))
        return "image/jpeg";
    if (starts_with_bytes(bytes, {'R', 'I', 'F', 'F'}) && starts_with_bytes(bytes, {'W', 'E', 'B', 'P'}, 8)) {
        return "image/webp";
    }
    if (starts_with_bytes(bytes, {'f', 't', 'y', 'p'}, 4))
        return "image/avif";
    if (starts_with_bytes(bytes, {'O', 'g', 'g', 'S'}))
        return "audio/ogg";
    if (ends(".json"))
        return "application/json";
    if (ends(".molang"))
        return "text/x-molang";
    if (ends(".txt") || ends(".md"))
        return "text/plain";
    if (ends(".geometry"))
        return "application/x-omm-geometry";
    return "application/octet-stream";
}

void finish_resources(Asset &asset, const std::unordered_map<std::string, std::pair<int32_t, int32_t>> &raw_images) {
    asset.resource_info.clear();
    asset.retained_bytes = 0;
    for (const Resource &resource : asset.resources.entries()) {
        ResourceInfo info;
        const auto raw = raw_images.find(resource.path);
        if (raw != raw_images.end()) {
            info.media_type = "image/x-omm-rgba8";
            info.width = raw->second.first;
            info.height = raw->second.second;
        } else {
            info.media_type = sniff_media_type(resource.bytes, resource.path);
        }
        asset.resource_info.push_back(std::move(info));
        asset.retained_bytes += resource.bytes.size();
    }
}

void parse_motion_files(Asset &asset, const Limits &limits) {
    const Manifest &manifest = asset.manifest;
    const std::unordered_set<std::string> first_person(manifest.first_person_animations.begin(),
                                                       manifest.first_person_animations.end());

    struct AnimationResult {
        std::optional<AnimationFile> file;
        std::vector<std::string> diagnostics;
    };

    struct ControllerResult {
        std::optional<ControllerFile> file;
        std::vector<std::string> diagnostics;
    };

    std::vector<AnimationResult> animations(manifest.animations.size());
    std::vector<ControllerResult> controllers(manifest.controllers.size());
    const size_t file_participants =
        std::max<size_t>(1, std::min<size_t>(limits.worker_count, animations.size() + controllers.size()));
    const uint32_t clip_threads = std::max(1u, limits.worker_count / static_cast<uint32_t>(file_participants));
    abi::parallel_for(animations.size() + controllers.size(), limits.worker_count, [&](size_t index) {
        // JSON limits are thread-local and must be installed on every execution thread.
        json::ScopedNestingLimit nesting(limits.nesting_limit);
        if (index >= animations.size()) {
            const size_t controller_index = index - animations.size();
            const std::string &path = manifest.controllers[controller_index];
            auto &result = controllers[controller_index];
            auto document = parse_resource_json(asset.resources, path, result.diagnostics);
            if (document) {
                result.file = parse_controllers(document->root(), path, manifest.merge_multiline_expressions);
            }
            return;
        }
        const std::string &path = manifest.animations[index];
        auto &result = animations[index];
        auto document = parse_resource_json(asset.resources, path, result.diagnostics);
        if (!document) {
            return;
        }
        result.file = parse_animations(document->root(), path, manifest.merge_multiline_expressions, clip_threads);
        result.file->first_person = result.file->first_person || first_person.count(path) != 0;
    });
    // Publish in manifest order: task completion order must not change events or diagnostics.
    for (auto &result : animations) {
        asset.diagnostics.insert(asset.diagnostics.end(), result.diagnostics.begin(), result.diagnostics.end());
        if (result.file) {
            const auto &errors = result.file->diagnostics;
            asset.diagnostics.insert(asset.diagnostics.end(), errors.begin(), errors.end());
            asset.animation_files.push_back(std::move(*result.file));
        }
    }
    for (auto &result : controllers) {
        asset.diagnostics.insert(asset.diagnostics.end(), result.diagnostics.begin(), result.diagnostics.end());
        if (result.file) {
            const auto &errors = result.file->diagnostics;
            asset.diagnostics.insert(asset.diagnostics.end(), errors.begin(), errors.end());
            asset.controller_files.push_back(std::move(*result.file));
        }
    }
}

// Raw layouts: manifest + JSON geometry + motion files.
std::unique_ptr<Asset> build_raw(ResourceSet resources, SourceInfo source, const std::string &display_name,
                                 const Limits &limits) {
    auto asset = std::make_unique<Asset>();
    asset->source = std::move(source);
    asset->resources = std::move(resources);
    asset->manifest = read_manifest(asset->resources, display_name, asset->metadata_json);
    asset->diagnostics = asset->manifest.warnings;

    for (const std::string &path : {asset->manifest.main_model, asset->manifest.arm_model}) {
        if (path.empty()) {
            continue;
        }
        std::vector<std::string> errors;
        auto document = parse_resource_json(asset->resources, path, errors);
        if (!document) {
            fail(DecodeCode::invalid_data, "geometry/" + path + ": " + (errors.empty() ? "" : errors.front()), 0);
        }
        asset->geometries.push_back(parse_geometry(document->root(), path));
    }
    parse_motion_files(*asset, limits);
    finish_resources(*asset, {});
    return asset;
}

} // namespace

SourceFormat probe(std::span<const uint8_t> input) noexcept {
    if (is_current_container(input)) {
        return SourceFormat::asset_container;
    }
    switch (probe_ysgp(input)) {
    case YsgpKind::legacy_v1:
        return SourceFormat::legacy_v1;
    case YsgpKind::legacy_v2:
        return SourceFormat::legacy_v2;
    case YsgpKind::v3:
        return SourceFormat::legacy_v3;
    case YsgpKind::unknown_version:
        return SourceFormat::unknown;
    case YsgpKind::none:
        break;
    }
    return is_zip(input) ? SourceFormat::zip : SourceFormat::unknown;
}

std::unique_ptr<Asset> parse_container(std::span<const uint8_t> input, const std::string &display_name,
                                       const Limits &limits) {
    if (is_current_container(input)) {
        return parse_current_asset(input, display_name, limits);
    }
    Budget budget(limits);
    budget.length(static_cast<int64_t>(input.size()), limits.file_bytes, "source/file", 0);
    const YsgpKind ysgp = probe_ysgp(input);
    SourceInfo source;
    if (ysgp == YsgpKind::legacy_v1 || ysgp == YsgpKind::legacy_v2) {
        source.container = ysgp == YsgpKind::legacy_v1 ? SourceContainer::legacy_v1 : SourceContainer::legacy_v2;
        return build_raw(decode_legacy(input, budget), source, display_name, limits);
    }
    if (ysgp == YsgpKind::v3) {
        std::vector<uint8_t> payload = decode_v3(input, budget);
        binary::Model model;
        try {
            model = binary::read_payload(payload, budget);
        } catch (...) {
            secure_clear(payload);
            throw;
        }
        secure_clear(payload);

        BinaryMapping mapping = map_binary(model, display_name, budget);
        auto asset = std::make_unique<Asset>();
        asset->source.container = SourceContainer::v3;
        asset->source.binary_format = model.format_version;
        asset->resources = std::move(mapping.resources);
        asset->manifest = std::move(mapping.manifest);
        asset->geometries = std::move(mapping.geometries);
        asset->metadata_json = std::move(mapping.metadata_json);
        asset->diagnostics = asset->manifest.warnings;
        parse_motion_files(*asset, limits);
        finish_resources(*asset, mapping.raw_images);
        return asset;
    }
    if (ysgp == YsgpKind::unknown_version) {
        fail(DecodeCode::unsupported_version, "source/crypto", 0);
    }
    if (is_zip(input)) {
        source.container = SourceContainer::zip;
        ResourceSet resources = resources_from_zip(input, budget, source.zip_root);
        return build_raw(std::move(resources), source, display_name, limits);
    }
    fail(DecodeCode::unsupported_version, "source/unrecognized-format", 0);
}

std::unique_ptr<Asset> parse_entries(std::span<const EntryInput> entries, const std::string &display_name,
                                     const Limits &limits) {
    Budget budget(limits);
    SourceInfo source;
    source.container = SourceContainer::directory;
    return build_raw(resources_from_entries(entries, budget), source, display_name, limits);
}

// ---------------------------------------------------------------------------
// Section writer
// ---------------------------------------------------------------------------
namespace {

class Writer {
public:
    explicit Writer(const Asset &asset) : asset_(asset), doc_(yyjson_mut_doc_new(nullptr)) {
        if (doc_ == nullptr) {
            throw std::bad_alloc();
        }
        for (size_t index = 0; index < asset.resources.entries().size(); ++index) {
            ids_.emplace(asset.resources.entries()[index].path, static_cast<uint32_t>(index));
        }
    }

    ~Writer() {
        yyjson_mut_doc_free(doc_);
    }

    Writer(const Writer &) = delete;
    Writer &operator=(const Writer &) = delete;

    std::string write(Section section) {
        yyjson_mut_val *root = obj();
        put(root, "schema_version", yyjson_mut_uint(doc_, 1));
        yyjson_mut_val *records = arr();
        switch (section) {
        case Section::metadata:
            metadata(records);
            break;
        case Section::animations:
            animations(records);
            break;
        case Section::controllers:
            controllers(records);
            break;
        case Section::expressions:
            expressions(records);
            break;
        case Section::resources:
            resources(records);
            break;
        case Section::diagnostics:
            diagnostics(records);
            break;
        default:
            throw abi::Failure(omm_error_argument, omm_stage_parse, "unknown asset section");
        }
        put(root, "records", records);
        yyjson_mut_doc_set_root(doc_, root);
        size_t length = 0;
        char *text = yyjson_mut_write(doc_, 0, &length);
        if (text == nullptr) {
            throw std::bad_alloc();
        }
        std::string result(text, length);
        free(text);
        return result;
    }

private:
    yyjson_mut_val *obj() {
        return yyjson_mut_obj(doc_);
    }

    yyjson_mut_val *arr() {
        return yyjson_mut_arr(doc_);
    }

    yyjson_mut_val *str(const std::string &value) {
        // Strings from lenient sources were already U+FFFD-sanitized or validated.
        return yyjson_mut_strncpy(doc_, value.data(), value.size());
    }

    yyjson_mut_val *num(float value) {
        if (!std::isfinite(value)) {
            return yyjson_mut_null(doc_); // never emit non-standard Infinity/NaN
        }
        const std::string text = json::float_text(value);
        return yyjson_mut_rawncpy(doc_, text.data(), text.size());
    }

    yyjson_mut_val *integer(int64_t value) {
        return yyjson_mut_sint(doc_, value);
    }

    yyjson_mut_val *boolean(bool value) {
        return yyjson_mut_bool(doc_, value);
    }

    void put(yyjson_mut_val *object, const char *key, yyjson_mut_val *value) {
        yyjson_mut_obj_add(object, yyjson_mut_strcpy(doc_, key), value);
    }

    void push(yyjson_mut_val *array, yyjson_mut_val *value) {
        yyjson_mut_arr_append(array, value);
    }

    yyjson_mut_val *strings(const std::vector<std::string> &values) {
        yyjson_mut_val *array = arr();
        for (const std::string &value : values) {
            push(array, str(value));
        }
        return array;
    }

    // Resource reference: stable uint32 id, or null when the path is not in the asset.
    yyjson_mut_val *ref(const std::string &path) {
        if (path.empty()) {
            return yyjson_mut_null(doc_);
        }
        const Resource *resource = asset_.resources.find(path);
        if (resource == nullptr) {
            return yyjson_mut_null(doc_);
        }
        return yyjson_mut_uint(doc_, ids_.at(resource->path));
    }

    yyjson_mut_val *refs(const std::vector<std::string> &paths) {
        yyjson_mut_val *array = arr();
        for (const std::string &path : paths) {
            yyjson_mut_val *entry = obj();
            put(entry, "path", str(path));
            put(entry, "resource", ref(path));
            push(array, entry);
        }
        return array;
    }

    void metadata(yyjson_mut_val *records) {
        const Manifest &m = asset_.manifest;
        yyjson_mut_val *record = obj();
        yyjson_mut_val *source = obj();
        static const char *kContainers[] = {"directory", "zip", "legacy_v1", "legacy_v2", "v3", "current"};
        put(source, "container", str(kContainers[static_cast<int>(asset_.source.container)]));
        put(source, "binary_format", integer(asset_.source.binary_format));
        put(source, "zip_root", str(asset_.source.zip_root));
        put(record, "source", source);
        put(record, "kind", str(m.kind == AvatarKind::current ? "current" : "legacy"));
        put(record, "name", str(m.name));
        put(record, "description", str(m.description));
        put(record, "authors", strings(m.authors));
        put(record, "main_model", ref(m.main_model));
        put(record, "arm_model", ref(m.arm_model));
        put(record, "default_texture", str(m.default_texture));
        put(record, "free", boolean(m.free));
        put(record, "merge_multiline_expressions", boolean(m.merge_multiline_expressions));
        yyjson_mut_val *textures = arr();
        for (const TextureOption &texture : m.textures) {
            yyjson_mut_val *entry = obj();
            put(entry, "id", str(texture.id));
            put(entry, "name", str(texture.name));
            put(entry, "path", str(texture.path));
            put(entry, "resource", ref(texture.path));
            put(entry, "normal_resource", ref(texture.normal_path));
            put(entry, "specular_resource", ref(texture.specular_path));
            push(textures, entry);
        }
        put(record, "textures", textures);
        put(record, "animations", refs(m.animations));
        put(record, "controllers", refs(m.controllers));
        put(record, "functions", refs(m.functions));
        put(record, "sounds", refs(m.sounds));
        put(record, "languages", refs(m.languages));
        put(record, "projectiles", refs(m.projectiles));
        put(record, "vehicles", refs(m.vehicles));
        put(record, "avatar_image", ref(m.avatar_image));
        put(record, "background_image", ref(m.background_image));
        if (!m.gui_foreground.empty()) {
            put(record, "gui_foreground", ref(m.gui_foreground));
        }
        if (!m.gui_background.empty()) {
            put(record, "gui_background", ref(m.gui_background));
        }
        if (!m.thumbnail_image.empty()) {
            put(record, "thumbnail_image", ref(m.thumbnail_image));
        }
        yyjson_mut_val *geometries = arr();
        for (size_t index = 0; index < asset_.geometries.size(); ++index) {
            const Geometry &geometry = asset_.geometries[index];
            size_t quads = 0;
            for (const GeometryBone &bone : geometry.bones) {
                for (const GeometryCube &cube : bone.cubes) {
                    quads += cube.quads.size();
                }
            }
            yyjson_mut_val *entry = obj();
            put(entry, "id", integer(static_cast<int64_t>(index)));
            put(entry, "path", str(geometry.path));
            put(entry, "texture_width", integer(geometry.texture_width));
            put(entry, "texture_height", integer(geometry.texture_height));
            put(entry, "bones", integer(static_cast<int64_t>(geometry.bones.size())));
            put(entry, "quads", integer(static_cast<int64_t>(quads)));
            push(geometries, entry);
        }
        put(record, "geometries", geometries);
        // Source manifest, verbatim structure (ysm.json, ysm_extra_info or the binary mapping).
        auto manifest = json::Document::parse(asset_.metadata_json.empty() ? "{}" : asset_.metadata_json);
        put(record, "manifest", manifest ? yyjson_val_mut_copy(doc_, manifest->root().raw()) : obj());
        push(records, record);
    }

    yyjson_mut_val *program(const ProgramRef &value) {
        yyjson_mut_val *entry = obj();
        put(entry, "format", integer(value->format));
        if (value->source) {
            put(entry, "source", str(*value->source));
        } else {
            static constexpr char hex[] = "0123456789abcdef";
            std::string encoded;
            encoded.reserve(value->bytecode.size() * 2);
            for (uint8_t byte : value->bytecode) {
                encoded.push_back(hex[byte >> 4]);
                encoded.push_back(hex[byte & 15]);
            }
            put(entry, "bytecode_hex", str(encoded));
        }
        return entry;
    }

    yyjson_mut_val *component(const Component &value) {
        yyjson_mut_val *entry = obj();
        put(entry, "value", num(value.value));
        if (value.expression) {
            put(entry, "expression", str(*value.expression));
        }
        if (value.program) {
            put(entry, "program", program(value.program));
        }
        return entry;
    }

    yyjson_mut_val *vector(const ComponentVector &values) {
        yyjson_mut_val *array = arr();
        for (const Component &value : values) {
            push(array, component(value));
        }
        return array;
    }

    yyjson_mut_val *channel(const std::optional<Channel> &channel) {
        if (!channel) {
            return yyjson_mut_null(doc_);
        }
        yyjson_mut_val *entry = obj();
        if (!channel->keyframed) {
            put(entry, "constant", vector(channel->constant));
            return entry;
        }
        yyjson_mut_val *keyframes = arr();
        for (const Keyframe &keyframe : channel->keyframes) {
            yyjson_mut_val *frame = obj();
            put(frame, "ordinal", integer(keyframe.ordinal));
            put(frame, "time", num(keyframe.time));
            put(frame, "interpolation", str(keyframe.interpolation));
            put(frame, "post", vector(keyframe.post));
            put(frame, "pre", vector(keyframe.pre));
            push(keyframes, frame);
        }
        put(entry, "keyframes", keyframes);
        return entry;
    }

    void animations(yyjson_mut_val *records) {
        static const char *kLoops[] = {"once", "loop", "hold_on_last_frame"};
        for (size_t index = 0; index < asset_.animation_files.size(); ++index) {
            const AnimationFile &file = asset_.animation_files[index];
            yyjson_mut_val *record = obj();
            put(record, "id", integer(static_cast<int64_t>(index)));
            put(record, "path", str(file.path));
            put(record, "resource", ref(file.path));
            put(record, "first_person", boolean(file.first_person));
            yyjson_mut_val *clips = arr();
            for (const AnimationClip &clip : file.clips) {
                yyjson_mut_val *entry = obj();
                put(entry, "name", str(clip.name));
                // Unbounded clips carry no finite length (the reference uses Float.MAX_VALUE).
                put(entry, "length", clip.unbounded ? yyjson_mut_null(doc_) : num(clip.length));
                put(entry, "unbounded", boolean(clip.unbounded));
                put(entry, "loop", str(kLoops[static_cast<int>(clip.loop)]));
                yyjson_mut_val *blend = obj();
                put(blend, "value", num(clip.blend_weight));
                if (clip.blend_weight_expression) {
                    put(blend, "expression", str(*clip.blend_weight_expression));
                }
                if (clip.blend_weight_program) {
                    put(blend, "program", program(clip.blend_weight_program));
                }
                put(entry, "blend_weight", blend);
                yyjson_mut_val *bones = arr();
                for (const BoneAnimation &bone : clip.bones) {
                    yyjson_mut_val *bone_entry = obj();
                    put(bone_entry, "name", str(bone.bone));
                    put(bone_entry, "position", channel(bone.position));
                    put(bone_entry, "rotation", channel(bone.rotation));
                    put(bone_entry, "scale", channel(bone.scale));
                    push(bones, bone_entry);
                }
                put(entry, "bones", bones);
                yyjson_mut_val *events = arr();
                for (const AnimationEvent &event : clip.events) {
                    yyjson_mut_val *event_entry = obj();
                    put(event_entry, "ordinal", integer(event.ordinal));
                    put(event_entry, "time", num(event.time));
                    put(event_entry, "type", str(event.type));
                    put(event_entry, "text", str(event.text));
                    if (event.program) {
                        put(event_entry, "program", program(event.program));
                    }
                    yyjson_mut_val *params = obj();
                    for (const auto &param : event.params) {
                        yyjson_mut_obj_add(params, str(param.first), str(param.second));
                    }
                    put(event_entry, "params", params);
                    push(events, event_entry);
                }
                put(entry, "events", events);
                push(clips, entry);
            }
            put(record, "clips", clips);
            push(records, record);
        }
    }

    yyjson_mut_val *condition(const std::optional<std::string> &text) {
        return text ? str(*text) : yyjson_mut_null(doc_);
    }

    void controllers(yyjson_mut_val *records) {
        for (size_t index = 0; index < asset_.controller_files.size(); ++index) {
            const ControllerFile &file = asset_.controller_files[index];
            yyjson_mut_val *record = obj();
            put(record, "id", integer(static_cast<int64_t>(index)));
            put(record, "path", str(file.path));
            put(record, "resource", ref(file.path));
            yyjson_mut_val *controllers = arr();
            for (const Controller &controller : file.controllers) {
                yyjson_mut_val *entry = obj();
                put(entry, "name", str(controller.name));
                put(entry, "initial_state", str(controller.initial_state));
                yyjson_mut_val *states = arr();
                for (const ControllerState &state : controller.states) {
                    yyjson_mut_val *state_entry = obj();
                    put(state_entry, "name", str(state.name));
                    yyjson_mut_val *animations = arr();
                    for (const ControllerAnimationRef &animation : state.animations) {
                        yyjson_mut_val *item = obj();
                        put(item, "name", str(animation.animation));
                        put(item, "weight", condition(animation.weight));
                        if (animation.program) {
                            put(item, "program", program(animation.program));
                        }
                        push(animations, item);
                    }
                    put(state_entry, "animations", animations);
                    yyjson_mut_val *transitions = arr();
                    for (const ControllerTransition &transition : state.transitions) {
                        yyjson_mut_val *item = obj();
                        put(item, "target", str(transition.target));
                        put(item, "condition", condition(transition.condition));
                        if (transition.program) {
                            put(item, "program", program(transition.program));
                        }
                        push(transitions, item);
                    }
                    put(state_entry, "transitions", transitions);
                    put(state_entry, "on_entry", strings(state.on_entry));
                    put(state_entry, "on_exit", strings(state.on_exit));
                    if (state.entry_program) {
                        put(state_entry, "entry_program", program(state.entry_program));
                    }
                    if (state.exit_program) {
                        put(state_entry, "exit_program", program(state.exit_program));
                    }
                    put(state_entry, "blend_transition", num(state.blend_transition));
                    if (state.blend_curve) {
                        yyjson_mut_val *curve = arr();
                        for (const auto &point : state.blend_points) {
                            yyjson_mut_val *pair = arr();
                            push(pair, num(point.first));
                            push(pair, num(point.second));
                            push(curve, pair);
                        }
                        put(state_entry, "blend_curve", curve);
                    } else {
                        put(state_entry, "blend_curve", yyjson_mut_null(doc_));
                    }
                    put(state_entry, "blend_via_shortest_path", boolean(state.blend_via_shortest_path));
                    put(state_entry, "sound_effects", strings(state.sound_effects));
                    push(states, state_entry);
                }
                put(entry, "states", states);
                push(controllers, entry);
            }
            put(record, "controllers", controllers);
            push(records, record);
        }
    }

    void expressions(yyjson_mut_val *records) {
        uint32_t id = 0;
        for (const NamedProgram &named : asset_.programs) {
            yyjson_mut_val *record = obj();
            put(record, "id", integer(id++));
            put(record, "role", str("user_function"));
            put(record, "language", str("portable_program"));
            put(record, "name", str(named.name));
            put(record, "program", program(named.program));
            push(records, record);
        }
        for (const std::string &path : asset_.manifest.functions) {
            const Resource *resource = asset_.resources.find(path);
            if (resource == nullptr) {
                continue;
            }
            yyjson_mut_val *record = obj();
            put(record, "id", integer(id++));
            put(record, "role", str("function"));
            put(record, "language", str("molang"));
            put(record, "path", str(path));
            put(record, "resource", ref(path));
            put(record, "text", str(json::decode_utf8_lenient(resource->bytes)));
            push(records, record);
        }
    }

    void resources(yyjson_mut_val *records) {
        const auto &entries = asset_.resources.entries();
        for (size_t index = 0; index < entries.size(); ++index) {
            const ResourceInfo &info = asset_.resource_info[index];
            yyjson_mut_val *record = obj();
            put(record, "id", integer(static_cast<int64_t>(index)));
            put(record, "path", str(entries[index].path));
            put(record, "media_type", str(info.media_type));
            put(record, "bytes", integer(static_cast<int64_t>(entries[index].bytes.size())));
            if (info.width > 0) {
                put(record, "width", integer(info.width));
                put(record, "height", integer(info.height));
            }
            push(records, record);
        }
    }

    void diagnostics(yyjson_mut_val *records) {
        for (const std::string &message : asset_.diagnostics) {
            yyjson_mut_val *record = obj();
            put(record, "severity", str("warning"));
            put(record, "message", str(message));
            push(records, record);
        }
    }

    const Asset &asset_;
    yyjson_mut_doc *doc_;
    std::unordered_map<std::string, uint32_t> ids_;
};

} // namespace

std::string write_section(const Asset &asset, Section section) {
    Writer writer(asset);
    return writer.write(section);
}

} // namespace omm::ysm
