#include "ysmparser/binary_mapper.hpp"

#include <cmath>
#include <unordered_set>

#include "ysmparser/json.hpp"
#include "ysmparser/normalize.hpp"
#include "yyjson.h"

namespace omm::ysm {

namespace {

[[noreturn]] void invalid(const char *stage) {
    fail(DecodeCode::invalid_data, stage, 0);
}

bool is_blank(const std::string &text) {
    for (char c : text) {
        if (!(c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v')) {
            return false;
        }
    }
    return true;
}

std::string safe_name(const std::string &value) {
    const auto normalized = normalize_relative(value);
    if (!normalized || normalized->empty()) {
        invalid("mapping/resource-name");
    }
    return *normalized;
}

// Thin RAII + builder over a yyjson mutable document. Floats are written as raw shortest
// round-trip text, which re-parses to the identical float (the reference writes then re-reads).
class JsonBuilder {
public:
    JsonBuilder() : doc_(yyjson_mut_doc_new(nullptr)) {
        if (doc_ == nullptr) {
            throw std::bad_alloc();
        }
    }

    ~JsonBuilder() {
        yyjson_mut_doc_free(doc_);
    }

    JsonBuilder(const JsonBuilder &) = delete;
    JsonBuilder &operator=(const JsonBuilder &) = delete;

    yyjson_mut_val *object() {
        return yyjson_mut_obj(doc_);
    }

    yyjson_mut_val *array() {
        return yyjson_mut_arr(doc_);
    }

    yyjson_mut_val *string(const std::string &value) {
        return yyjson_mut_strncpy(doc_, value.data(), value.size());
    }

    yyjson_mut_val *number(float value) {
        const std::string text = json::float_text(value);
        return yyjson_mut_rawncpy(doc_, text.data(), text.size());
    }

    yyjson_mut_val *integer(int64_t value) {
        return yyjson_mut_sint(doc_, value);
    }

    yyjson_mut_val *boolean(bool value) {
        return yyjson_mut_bool(doc_, value);
    }

    void put(yyjson_mut_val *object, const std::string &key, yyjson_mut_val *value) {
        yyjson_mut_obj_add(object, yyjson_mut_strncpy(doc_, key.data(), key.size()), value);
    }

    void put_string(yyjson_mut_val *object, const std::string &key, const std::string &value) {
        put(object, key, string(value));
    }

    void append(yyjson_mut_val *array, yyjson_mut_val *value) {
        yyjson_mut_arr_append(array, value);
    }

    yyjson_mut_val *strings(const binary::OrderedMap<std::string> &values) {
        yyjson_mut_val *result = object();
        for (const auto &entry : values.entries()) {
            put_string(result, entry.first, entry.second);
        }
        return result;
    }

    yyjson_mut_val *string_list(const std::vector<std::string> &values) {
        yyjson_mut_val *result = array();
        for (const std::string &value : values) {
            append(result, string(value));
        }
        return result;
    }

    yyjson_mut_val *value(const binary::Value &item) {
        if (const float *number_value = std::get_if<float>(&item)) {
            if (!std::isfinite(*number_value)) {
                invalid("mapping/value-type");
            }
            return number(*number_value);
        }
        return string(std::get<std::string>(item));
    }

    std::vector<uint8_t> serialize(yyjson_mut_val *root) {
        yyjson_mut_doc_set_root(doc_, root);
        size_t length = 0;
        char *text = yyjson_mut_write(doc_, 0, &length);
        if (text == nullptr) {
            throw std::bad_alloc();
        }
        std::vector<uint8_t> bytes(text, text + length);
        free(text);
        return bytes;
    }

private:
    yyjson_mut_doc *doc_;
};

// Object key uniqueness as enforced by the reference mapper (JsonObject.has before add).
class UniqueKeys {
public:
    void check(const std::string &key) {
        if (is_blank(key) || !keys_.insert(key).second) {
            invalid("mapping/duplicate-or-empty-name");
        }
    }

private:
    std::unordered_set<std::string> keys_;
};

bool is_avif(const std::vector<uint8_t> &bytes) {
    if (bytes.size() < 16 || bytes[4] != 'f' || bytes[5] != 't' || bytes[6] != 'y' || bytes[7] != 'p') {
        return false;
    }
    for (size_t offset = 8; offset <= std::min<size_t>(bytes.size() - 4, 60); offset += 4) {
        if (bytes[offset] == 'a' && bytes[offset + 1] == 'v' && bytes[offset + 2] == 'i' &&
            (bytes[offset + 3] == 'f' || bytes[offset + 3] == 's')) {
            return true;
        }
    }
    return false;
}

class Mapper {
public:
    Mapper(const binary::Model &model, const std::string &display_name, Budget &budget)
        : model_(model), display_name_(display_name), budget_(budget) {}

    BinaryMapping map() {
        JsonBuilder manifest_json;
        yyjson_mut_val *root = metadata(manifest_json);

        const std::string main = geometry(model_.main_entity.main_model, "binary/main.geometry");
        const std::string arm =
            model_.main_entity.arm_model ? geometry(model_.main_entity.arm_model, "binary/arm.geometry") : "";
        const std::vector<std::string> animations =
            animation_files(model_.main_entity.animation_files, "animations/player");
        const std::vector<std::string> controllers =
            controller_files(model_.main_entity.animation_controller_files, "controllers/player");
        const std::vector<TextureOption> textures = texture_options(model_.main_entity.textures, "textures/player");

        yyjson_mut_val *player = manifest_json.object();
        yyjson_mut_val *models = manifest_json.object();
        manifest_json.put_string(models, "main", main);
        manifest_json.put_string(models, "arm", arm);
        manifest_json.put(player, "model", models);
        manifest_json.put(player, "animation", manifest_json.string_list(animations));
        manifest_json.put(player, "animation_controllers", manifest_json.string_list(controllers));
        manifest_json.put(player, "texture", texture_definitions(manifest_json, textures));
        yyjson_mut_val *definitions = manifest_json.object();
        manifest_json.put(definitions, "player", player);
        std::vector<std::string> projectile_models;
        std::vector<std::string> vehicle_models;
        manifest_json.put(definitions, "projectiles",
                          entities(manifest_json, model_.projectiles, "projectile", projectile_models));
        manifest_json.put(definitions, "vehicles", entities(manifest_json, model_.vehicles, "vehicle", vehicle_models));
        manifest_json.put(root, "files", definitions);

        const std::vector<std::string> functions = data_files(model_.function_files, "functions");
        const std::vector<std::string> sounds = data_files(model_.sound_files, "sounds");
        std::vector<std::string> languages;
        for (const auto &entry : model_.language_files.entries()) {
            const std::string path = "lang/" + safe_name(entry.first) + ".json";
            JsonBuilder language;
            add_owned(path, language.serialize(language.strings(entry.second.data)));
            hashes_.emplace_back(path, entry.second.hash);
            languages.push_back(path);
        }
        yyjson_mut_val *hash_object = manifest_json.object();
        for (const auto &hash : hashes_) {
            manifest_json.put_string(hash_object, hash.first, hash.second);
        }
        manifest_json.put(root, "ysm_binary_resource_hashes", hash_object);
        std::vector<uint8_t> manifest_bytes = manifest_json.serialize(root);
        result_.metadata_json.assign(manifest_bytes.begin(), manifest_bytes.end());
        add_owned("ysm.json", std::move(manifest_bytes));

        Manifest &manifest = result_.manifest;
        manifest.kind = AvatarKind::current;
        manifest.name = is_blank(model_.metadata.name) ? display_name_ : model_.metadata.name;
        manifest.description = model_.metadata.tips;
        for (const auto &author : model_.metadata.authors) {
            manifest.authors.push_back(author.name);
        }
        manifest.main_model = main;
        manifest.arm_model = arm;
        manifest.animations = animations;
        manifest.controllers = controllers;
        manifest.textures = textures;
        manifest.default_texture = model_.properties.default_texture;
        bool default_found = false;
        for (const TextureOption &texture : textures) {
            default_found = default_found || texture.id == model_.properties.default_texture;
        }
        if (!textures.empty() && !default_found) {
            manifest.default_texture = textures.front().id;
        }
        manifest.free = model_.properties.is_free;
        manifest.merge_multiline_expressions = model_.properties.merge_multiline_expr;
        manifest.functions = functions;
        manifest.sounds = sounds;
        manifest.languages = languages;
        manifest.projectiles = projectile_models;
        manifest.vehicles = vehicle_models;
        manifest.background_image = model_.properties.gui_background;
        manifest.warnings = warnings_;
        return std::move(result_);
    }

private:
    void add_owned(const std::string &path, std::vector<uint8_t> bytes) {
        budget_.begin_resource("mapping/resource-count", 0);
        budget_.resource(bytes.size(), "mapping/resource-bytes", 0);
        if (result_.resources.exists(path)) {
            invalid("mapping/duplicate-resource");
        }
        result_.resources.add(path, std::move(bytes), "mapping/resource");
    }

    void add(const std::string &path, const std::vector<uint8_t> &bytes) {
        budget_.length(static_cast<int64_t>(bytes.size()), budget_.limits().single_resource_bytes, "mapping/resource",
                       0);
        add_owned(path, bytes);
    }

    std::string image(JsonBuilder &json, yyjson_mut_val *descriptors, const binary::Image &raw) {
        budget_.check("mapping/metadata-image", 0);
        if (raw.data.empty()) {
            return std::string();
        }
        const std::string stem = "metadata/images/" + std::to_string(image_index_++);
        std::string path;
        if (raw.format == 5 && is_avif(raw.data)) {
            budget_.length(static_cast<int64_t>(raw.width) * raw.height, Budget::kMaxTexturePixels,
                           "mapping/encoded-metadata-pixels", 0);
            path = stem + ".avif";
            add(path, raw.data);
            warnings_.push_back("UI_SKIPPED_BY_USER: AVIF metadata author image " + path);
        } else {
            path = stem + ".encoded";
            add(path, raw.data);
            if (raw.format == -1) {
                result_.raw_images.emplace(path, std::make_pair(raw.width, raw.height));
            }
        }
        yyjson_mut_val *descriptor = json.object();
        json.put_string(descriptor, "name", raw.name);
        json.put_string(descriptor, "path", path);
        json.put(descriptor, "width", json.integer(raw.width));
        json.put(descriptor, "height", json.integer(raw.height));
        json.put(descriptor, "encoding", json.integer(raw.format));
        json.put(descriptor, "unknown_flag", json.integer(raw.unknown_flag));
        json.append(descriptors, descriptor);
        return path;
    }

    yyjson_mut_val *metadata(JsonBuilder &json) {
        budget_.check("mapping/metadata", 0);
        yyjson_mut_val *result = json.object();
        json.put(result, "spec", json.integer(2));
        yyjson_mut_val *metadata = json.object();
        json.put_string(metadata, "name", model_.metadata.name);
        json.put_string(metadata, "tips", model_.metadata.tips);
        yyjson_mut_val *license = json.object();
        json.put_string(license, "type", model_.metadata.license_type);
        json.put_string(license, "description", model_.metadata.license_description);
        json.put(metadata, "license", license);
        json.put(metadata, "link", json.strings(model_.metadata.links));
        yyjson_mut_val *authors = json.array();
        yyjson_mut_val *descriptors = json.array();
        for (const binary::Author &raw : model_.metadata.authors) {
            budget_.check("mapping/author", 0);
            yyjson_mut_val *author = json.object();
            json.put_string(author, "name", raw.name);
            json.put_string(author, "role", raw.role);
            json.put_string(author, "comment", raw.comment);
            json.put(author, "contact", json.strings(raw.contacts));
            std::string avatar = raw.avatar;
            if (raw.avatar_image && !model_.images[*raw.avatar_image].data.empty()) {
                avatar = image(json, descriptors, model_.images[*raw.avatar_image]);
            }
            json.put_string(author, "avatar", avatar);
            json.append(authors, author);
        }
        json.put(metadata, "authors", authors);
        yyjson_mut_val *extra_avatars = json.array();
        for (size_t index : model_.metadata.extra_avatars) {
            json.append(extra_avatars, json.string(image(json, descriptors, model_.images[index])));
        }
        json.put(metadata, "ysm_extra_avatars", extra_avatars);
        json.put(result, "metadata", metadata);

        const binary::Properties &raw = model_.properties;
        yyjson_mut_val *properties = json.object();
        json.put(properties, "width_scale", json.number(raw.width_scale));
        json.put(properties, "height_scale", json.number(raw.height_scale));
        json.put_string(properties, "default_texture", raw.default_texture);
        json.put_string(properties, "preview_animation", raw.preview_animation);
        json.put(properties, "free", json.boolean(raw.is_free));
        json.put(properties, "render_layers_first", json.boolean(raw.render_layers_first));
        json.put(properties, "all_cutout", json.boolean(raw.all_cutout));
        json.put(properties, "disable_preview_rotation", json.boolean(raw.disable_preview_rotation));
        json.put(properties, "gui_no_lighting", json.boolean(raw.gui_no_lighting));
        json.put(properties, "merge_multiline_expr", json.boolean(raw.merge_multiline_expr));
        json.put_string(properties, "gui_foreground", raw.gui_foreground);
        json.put_string(properties, "gui_background", raw.gui_background);
        json.put(properties, "extra_animation", json.strings(raw.extra_animations));
        yyjson_mut_val *classifications = json.array();
        for (const auto &classification : raw.extra_animation_classifies) {
            yyjson_mut_val *object = json.object();
            json.put_string(object, "id", classification.id);
            json.put(object, "extra_animation", json.strings(classification.extras));
            json.append(classifications, object);
        }
        json.put(properties, "extra_animation_classify", classifications);
        yyjson_mut_val *buttons = json.array();
        for (const auto &button : raw.extra_animation_buttons) {
            yyjson_mut_val *object = json.object();
            json.put_string(object, "id", button.id);
            json.put_string(object, "name", button.name);
            // The reference never reads a button description: Gson omits the null property.
            yyjson_mut_val *forms = json.array();
            for (const auto &form : button.forms) {
                yyjson_mut_val *config = json.object();
                json.put_string(config, "type", form.type);
                json.put_string(config, "title", form.title);
                json.put_string(config, "description", form.description);
                json.put_string(config, "value", form.default_value);
                json.put(config, "step", json.number(form.step));
                json.put(config, "min", json.number(form.min));
                json.put(config, "max", json.number(form.max));
                json.put(config, "labels", json.strings(form.labels));
                json.append(forms, config);
            }
            json.put(object, "config_forms", forms);
            json.append(buttons, object);
        }
        json.put(properties, "extra_animation_buttons", buttons);
        yyjson_mut_val *backgrounds = json.object();
        UniqueKeys background_names;
        for (size_t index : raw.background_images) {
            const binary::Image &background = model_.images[index];
            const std::string path = image(json, descriptors, background);
            if (!background_names_seen_.insert(background.name).second) {
                fail(DecodeCode::invalid_data, "mapping/duplicate-background", 0);
            }
            json.put_string(backgrounds, background.name, path);
        }
        json.put(properties, "ysm_background_images", backgrounds);
        json.put(result, "properties", properties);
        json.put(result, "ysm_binary_images", descriptors);

        yyjson_mut_val *footer = json.object();
        json.put(footer, "format_version", json.integer(model_.format_version));
        json.put_string(footer, "resource_hash", raw.sha256);
        json.put(footer, "version", json.integer(model_.footer.version));
        json.put(footer, "unknown_1", json.integer(model_.footer.unk_int1));
        json.put(footer, "unknown_2", json.integer(model_.footer.unk_int2));
        json.put_string(footer, "random", model_.footer.rand);
        json.put(footer, "time", json.integer(model_.footer.time));
        json.put_string(footer, "extra", model_.footer.extra);
        json.put(result, "ysm_binary_footer", footer);
        return result;
    }

    std::string geometry(const std::optional<binary::Geometry> &raw, const std::string &path) {
        if (!raw) {
            fail(DecodeCode::invalid_data, "geometry/missing", 0);
        }
        for (const Geometry &existing : result_.geometries) {
            if (existing.path == path) {
                invalid("mapping/geometry-path");
            }
        }
        result_.geometries.push_back(map_geometry(*raw, path, budget_));
        hashes_.emplace_back(path, raw->sha256);

        JsonBuilder json;
        yyjson_mut_val *metadata = json.object();
        json.put_string(metadata, "identifier", raw->identifier);
        json.put(metadata, "model_type", json.integer(raw->model_type));
        json.put(metadata, "visible_bounds_width", json.number(raw->visible_bounds_width));
        json.put(metadata, "visible_bounds_height", json.number(raw->visible_bounds_height));
        yyjson_mut_val *offset = json.array();
        for (float value : raw->visible_bounds_offset) {
            json.append(offset, json.number(value));
        }
        json.put(metadata, "visible_bounds_offset", offset);
        yyjson_mut_val *unknown = json.array();
        json.append(unknown, json.number(raw->unk_float1));
        json.append(unknown, json.number(raw->unk_float2));
        json.put(metadata, "unknown_floats", unknown);
        yyjson_mut_val *padding = json.array();
        for (int32_t pad : raw->footer_pad) {
            json.append(padding, json.integer(pad));
        }
        json.put(metadata, "footer_padding", padding);
        yyjson_mut_val *bones = json.array();
        for (const binary::Bone &bone : raw->bones) {
            yyjson_mut_val *descriptor = json.object();
            json.put_string(descriptor, "name", bone.name);
            yyjson_mut_val *pads = json.array();
            for (int32_t pad : bone.unk_pad) {
                json.append(pads, json.integer(pad));
            }
            json.put(descriptor, "unknown_padding", pads);
            yyjson_mut_val *cubes = json.array();
            for (const binary::Cube &cube : bone.cubes) {
                yyjson_mut_val *cube_pads = json.array();
                json.append(cube_pads, json.integer(cube.unk_int1));
                json.append(cube_pads, json.integer(cube.unk_int2));
                json.append(cube_pads, json.integer(cube.unk_int3));
                json.append(cubes, cube_pads);
            }
            json.put(descriptor, "cube_padding", cubes);
            json.append(bones, descriptor);
        }
        json.put(metadata, "bones", bones);
        add_owned(path + ".metadata.json", json.serialize(metadata));
        return path;
    }

    std::vector<TextureOption> texture_options(const binary::OrderedMap<binary::Texture> &textures,
                                               const std::string &prefix) {
        std::vector<TextureOption> options;
        std::unordered_set<std::string> ids;
        int index = 0;
        for (const auto &entry : textures.entries()) {
            const binary::Texture &texture = entry.second;
            if (is_blank(texture.name) || !ids.insert(lower_ascii(texture.name)).second) {
                invalid("mapping/texture-id");
            }
            const std::string stem = prefix + "/" + std::to_string(index++);
            const std::string path = stem + ".encoded";
            add(path, texture.data);
            hashes_.emplace_back(path, texture.hash.value_or(""));
            if (texture.image_format == -1) {
                result_.raw_images.emplace(path, std::make_pair(texture.width, texture.height));
            }

            JsonBuilder json;
            yyjson_mut_val *material = json.object();
            json.put_string(material, "id", texture.name);
            json.put_string(material, "path", path);
            json.put_string(material, "encoded_path", path);
            json.put(material, "encoding", json.integer(texture.image_format));
            json.put(material, "width", json.integer(texture.width));
            json.put(material, "height", json.integer(texture.height));
            json.put(material, "unknown_flag", json.integer(texture.unknown_flag));
            yyjson_mut_val *subs = json.array();
            std::string normal_path;
            std::string specular_path;
            int sub_index = 0;
            for (const binary::SubTexture &sub : texture.sub_textures) {
                const std::string sub_path = stem + "_sub_" + std::to_string(sub_index++) + ".encoded";
                add(sub_path, sub.data);
                hashes_.emplace_back(sub_path, sub.hash.value_or(""));
                if (sub.image_format == -1) {
                    result_.raw_images.emplace(sub_path, std::make_pair(sub.width, sub.height));
                }
                if (sub.specular_type == 1) {
                    normal_path = sub_path;
                } else if (sub.specular_type == 2) {
                    specular_path = sub_path;
                }
                yyjson_mut_val *descriptor = json.object();
                json.put(descriptor, "type", json.integer(sub.specular_type));
                json.put_string(descriptor, "path", sub_path);
                json.put_string(descriptor, "encoded_path", sub_path);
                json.put(descriptor, "encoding", json.integer(sub.image_format));
                json.put(descriptor, "width", json.integer(sub.width));
                json.put(descriptor, "height", json.integer(sub.height));
                json.put(descriptor, "unknown_flag", json.integer(sub.unknown_flag));
                json.append(subs, descriptor);
            }
            json.put(material, "sub_textures", subs);
            add_owned(stem + ".material.json", json.serialize(material));
            options.push_back(TextureOption{texture.name, texture.name, path, normal_path, specular_path});
        }
        return options;
    }

    yyjson_mut_val *texture_definitions(JsonBuilder &json, const std::vector<TextureOption> &textures) {
        yyjson_mut_val *definitions = json.array();
        for (const TextureOption &texture : textures) {
            yyjson_mut_val *definition = json.object();
            json.put_string(definition, "id", texture.id);
            json.put_string(definition, "name", texture.name);
            json.put_string(definition, "uv", texture.path);
            if (!is_blank(texture.normal_path)) {
                json.put_string(definition, "normal", texture.normal_path);
            }
            if (!is_blank(texture.specular_path)) {
                json.put_string(definition, "specular", texture.specular_path);
            }
            json.append(definitions, definition);
        }
        return definitions;
    }

    yyjson_mut_val *keyframe_channel(JsonBuilder &json, const std::vector<binary::Keyframe> &frames) {
        yyjson_mut_val *channel = json.object();
        UniqueKeys keys;
        for (const binary::Keyframe &raw : frames) {
            budget_.check("mapping/keyframe", 0);
            yyjson_mut_val *frame = json.object();
            const char *mode;
            switch (raw.interpolation_mode) {
            case 0:
                mode = "linear";
                break;
            case 1:
                mode = "step";
                break;
            case 2:
                mode = "catmullrom";
                break;
            case 3:
                mode = "bezier";
                break;
            default:
                invalid("mapping/interpolation");
            }
            json.put_string(frame, "lerp_mode", mode);
            yyjson_mut_val *post = json.array();
            for (const binary::Value &value : raw.post_data) {
                json.append(post, json.value(value));
            }
            json.put(frame, "post", post);
            if (raw.has_pre_data) {
                yyjson_mut_val *pre = json.array();
                for (const binary::Value &value : raw.pre_data) {
                    json.append(pre, json.value(value));
                }
                json.put(frame, "pre", pre);
            }
            json.put(frame, "bezier_linked", json.boolean(false));
            const std::string key = json::float_text(raw.timestamp);
            keys.check(key);
            json.put(channel, key, frame);
        }
        return channel;
    }

    yyjson_mut_val *events_at(JsonBuilder &json, yyjson_mut_val *events,
                              std::vector<std::pair<std::string, yyjson_mut_val *>> &index, float time) {
        if (!std::isfinite(time) || time < 0.0f) {
            invalid("mapping/event-time");
        }
        const std::string key = json::float_text(time);
        for (const auto &entry : index) {
            if (entry.first == key) {
                return entry.second;
            }
        }
        yyjson_mut_val *array = json.array();
        json.put(events, key, array);
        index.emplace_back(key, array);
        return array;
    }

    yyjson_mut_val *animation(JsonBuilder &json, const binary::Animation &raw) {
        budget_.check("mapping/animation", 0);
        yyjson_mut_val *animation = json.object();
        if (std::isfinite(raw.length)) {
            json.put(animation, "animation_length", json.number(raw.length));
        } else {
            json.put(animation, "ysm_duration_unbounded", json.boolean(true));
        }
        switch (raw.loop_mode) {
        case 0:
            json.put(animation, "loop", json.boolean(false));
            break;
        case 1:
            json.put(animation, "loop", json.boolean(true));
            break;
        case 2:
            break; // the reference encodes an absent loop property as 2
        case 3:
            json.put_string(animation, "loop", "hold_on_last_frame");
            break;
        default:
            invalid("mapping/loop-mode");
        }
        if (raw.blend_weight) {
            json.put(animation, "blend_weight", json.value(*raw.blend_weight));
        }
        yyjson_mut_val *bones = json.object();
        UniqueKeys bone_keys;
        for (const binary::BoneAnimation &raw_bone : raw.bone_animations) {
            yyjson_mut_val *bone = json.object();
            if (!raw_bone.rotation.empty()) {
                json.put(bone, "rotation", keyframe_channel(json, raw_bone.rotation));
            }
            if (!raw_bone.position.empty()) {
                json.put(bone, "position", keyframe_channel(json, raw_bone.position));
            }
            if (!raw_bone.scale.empty()) {
                json.put(bone, "scale", keyframe_channel(json, raw_bone.scale));
            }
            bone_keys.check(raw_bone.bone_name);
            json.put(bones, raw_bone.bone_name, bone);
        }
        json.put(animation, "bones", bones);

        yyjson_mut_val *timeline = json.object();
        std::vector<std::pair<std::string, yyjson_mut_val *>> timeline_index;
        for (const binary::TimelineEvent &event : raw.timeline_events) {
            yyjson_mut_val *events = events_at(json, timeline, timeline_index, event.timestamp);
            for (const std::string &expression : event.events) {
                json.append(events, json.string(expression));
            }
        }
        json.put(animation, "timeline", timeline);
        yyjson_mut_val *sounds = json.object();
        std::vector<std::pair<std::string, yyjson_mut_val *>> sound_index;
        for (const binary::SoundEffect &sound : raw.sound_effects) {
            yyjson_mut_val *effect = json.object();
            json.put_string(effect, "effect", sound.effect_name);
            json.append(events_at(json, sounds, sound_index, sound.timestamp), effect);
        }
        json.put(animation, "sound_effects", sounds);
        yyjson_mut_val *unknown = json.array();
        json.append(unknown, json.integer(raw.unk_int1));
        json.append(unknown, json.integer(raw.unk_int2));
        json.append(unknown, json.integer(raw.unk_int4));
        json.put(animation, "ysm_binary_fields", unknown);
        return animation;
    }

    std::vector<std::string> animation_files(const binary::OrderedMap<binary::AnimationFile> &files,
                                             const std::string &prefix) {
        std::vector<std::string> paths;
        for (const auto &entry : files.entries()) {
            int index = 0;
            std::unordered_set<std::string> names;
            // One clip per resource keeps large binary animation files within the per-resource budget.
            for (const auto &clip : entry.second.animations.entries()) {
                if (is_blank(clip.second.name) || !names.insert(clip.second.name).second) {
                    invalid("mapping/duplicate-or-empty-name");
                }
                JsonBuilder json;
                yyjson_mut_val *resource = json.object();
                yyjson_mut_val *clips = json.object();
                json.put(clips, clip.second.name, animation(json, clip.second));
                json.put(resource, "animations", clips);
                json.put(resource, "ysm_animation_type", json.integer(entry.second.anim_type));
                if (entry.second.file_hash) {
                    json.put_string(resource, "ysm_resource_hash", *entry.second.file_hash);
                }
                const std::string path =
                    prefix + "/" + safe_name(entry.first) + "/" + std::to_string(index++) + ".animation.json";
                add_owned(path, json.serialize(resource));
                hashes_.emplace_back(path, entry.second.file_hash.value_or(""));
                paths.push_back(path);
            }
        }
        return paths;
    }

    std::vector<std::string> controller_files(const std::vector<binary::ControllerFile> &files,
                                              const std::string &prefix) {
        std::vector<std::string> paths;
        for (size_t index = 0; index < files.size(); ++index) {
            const binary::ControllerFile &file = files[index];
            JsonBuilder json;
            yyjson_mut_val *controllers = json.object();
            UniqueKeys controller_keys;
            for (const auto &entry : file.controllers.entries()) {
                budget_.check("mapping/controller", 0);
                const binary::Controller &raw = entry.second;
                yyjson_mut_val *controller = json.object();
                json.put_string(controller, "initial_state", raw.initial_state);
                yyjson_mut_val *states = json.object();
                UniqueKeys state_keys;
                for (const binary::ControllerState &raw_state : raw.states) {
                    yyjson_mut_val *state = json.object();
                    json.put(state, "animations", json.strings(raw_state.animations));
                    json.put(state, "transitions", json.strings(raw_state.transitions));
                    json.put(state, "on_entry", json.string_list(raw_state.on_entry));
                    json.put(state, "on_exit", json.string_list(raw_state.on_exit));
                    json.put(state, "sound_effects", json.string_list(raw_state.sound_effects));
                    json.put(state, "blend_via_shortest_path", json.boolean(raw_state.blend_via_shortest_path));
                    if (raw_state.blend_transitions.empty()) {
                        json.put(state, "blend_transition", json.number(raw_state.blend_transition_value));
                    } else {
                        yyjson_mut_val *curve = json.object();
                        UniqueKeys point_keys;
                        for (const auto &point : raw_state.blend_transitions) {
                            const std::string key = json::float_text(point.first);
                            point_keys.check(key);
                            json.put(curve, key, json.number(point.second));
                        }
                        json.put(state, "blend_transition", curve);
                    }
                    state_keys.check(raw_state.name);
                    json.put(states, raw_state.name, state);
                }
                json.put(controller, "states", states);
                controller_keys.check(raw.animation_name);
                json.put(controllers, raw.animation_name, controller);
            }
            yyjson_mut_val *result = json.object();
            json.put(result, "animation_controllers", controllers);
            if (file.hash) {
                json.put_string(result, "ysm_resource_hash", *file.hash);
            }
            json.put(result, "ysm_legacy_unknown", json.integer(file.legacy_unknown_int));
            const std::string path = prefix + "/" + std::to_string(index) + ".controllers.json";
            add_owned(path, json.serialize(result));
            hashes_.emplace_back(path, file.hash.value_or(""));
            paths.push_back(path);
        }
        return paths;
    }

    yyjson_mut_val *entities(JsonBuilder &json, const binary::OrderedMap<binary::SubEntity> &entities,
                             const std::string &kind, std::vector<std::string> &model_paths) {
        yyjson_mut_val *definitions = json.array();
        int index = 0;
        for (const auto &entry : entities.entries()) {
            const binary::SubEntity &entity = entry.second;
            const std::string stem = "sub_entities/" + kind + "/" + std::to_string(index++);
            yyjson_mut_val *definition = json.object();
            json.put_string(definition, "id", entity.identifier);
            const std::string model_path = geometry(entity.model, stem + "/model.geometry");
            json.put_string(definition, "model", model_path);
            model_paths.push_back(model_path);
            json.put(definition, "match",
                     json.string_list(entity.match_ids.empty() ? std::vector<std::string>{entity.identifier}
                                                               : entity.match_ids));
            const std::vector<TextureOption> textures = texture_options(entity.textures, "textures/" + stem);
            json.put(definition, "texture", texture_definitions(json, textures));
            json.put(definition, "animation",
                     json.string_list(animation_files(entity.animation_files, "animations/" + stem)));
            json.put(definition, "animation_controllers",
                     json.string_list(controller_files(entity.animation_controller_files, "controllers/" + stem)));
            json.append(definitions, definition);
        }
        return definitions;
    }

    std::vector<std::string> data_files(const binary::OrderedMap<binary::DataFile> &files, const std::string &prefix) {
        std::vector<std::string> paths;
        for (const auto &entry : files.entries()) {
            const std::string path = prefix + "/" + safe_name(entry.first);
            add(path, entry.second.data);
            hashes_.emplace_back(path, entry.second.hash);
            paths.push_back(path);
        }
        return paths;
    }

    const binary::Model &model_;
    std::string display_name_;
    Budget &budget_;
    BinaryMapping result_;
    std::vector<std::pair<std::string, std::string>> hashes_;
    std::vector<std::string> warnings_;
    std::unordered_set<std::string> background_names_seen_;
    int image_index_ = 0;
};

} // namespace

BinaryMapping map_binary(const binary::Model &model, const std::string &display_name, Budget &budget) {
    Mapper mapper(model, display_name, budget);
    return mapper.map();
}

} // namespace omm::ysm
