#include "ysmparser/manifest.hpp"

#include <algorithm>

namespace omm::ysm {

namespace {

bool is_blank(const std::string &text) {
    return std::all_of(text.begin(), text.end(), [](char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
    });
}

bool ends_with(const std::string &text, std::string_view suffix) {
    return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool starts_with(const std::string &text, std::string_view prefix) {
    return text.compare(0, prefix.size(), prefix) == 0;
}

bool contains(const std::vector<std::string> &values, const std::string &value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}

std::string normalize(const std::string &path) {
    return normalize_relative(path).value_or(std::string());
}

std::string display_path(const std::string &path) {
    if (is_blank(path)) {
        return "<blank>";
    }
    std::string display = path;
    std::replace(display.begin(), display.end(), '\\', '/');
    return display;
}

void add_warning(std::vector<std::string> *warnings, const std::string &warning) {
    if (warnings != nullptr && !is_blank(warning) && !contains(*warnings, warning)) {
        warnings->push_back(warning);
    }
}

std::vector<std::string> resource_paths(const ResourceSet &resources) {
    std::vector<std::string> paths;
    paths.reserve(resources.size());
    for (const Resource &resource : resources.entries()) {
        paths.push_back(resource.path);
    }
    return paths;
}

class Reader {
public:
    Reader(const ResourceSet &resources, const std::string &display_name)
        : resources_(resources), display_name_(display_name), paths_(resource_paths(resources)) {}

    Manifest read(std::string &out_root_json) {
        if (resources_.exists("ysm.json")) {
            return read_new(out_root_json);
        }
        if (resources_.exists("main.json")) {
            const Resource *main = resources_.find("main.json");
            const std::string text = json::decode_utf8_lenient(main->bytes);
            if (text.find("minecraft:geometry") != std::string::npos) {
                return read_old(out_root_json);
            }
        }
        fail(DecodeCode::invalid_data, "UNRECOGNIZED_SOURCE: not a YSM avatar", 0);
    }

private:
    // YsmJson.parseObject: lenient; a non-object (or unparsable) root becomes {}.
    json::Value parse_object(const std::string &path) {
        const Resource *resource = resources_.find(path);
        const std::string text = json::decode_utf8_lenient(resource->bytes);
        std::string error;
        auto document = json::Document::parse(text, &error);
        if (!document) {
            fail(DecodeCode::invalid_data, "manifest/" + path + ": " + error, 0);
        }
        documents_.push_back(std::move(*document));
        const json::Value root = documents_.back().root();
        return root.is_object() ? root : json::Value();
    }

    bool exists(const std::string &path) const {
        return resources_.exists(path);
    }

    std::string existing_or_blank(const std::string &path) const {
        if (is_blank(path)) {
            return std::string();
        }
        const std::string normalized = normalize(path);
        return exists(normalized) ? normalized : std::string();
    }

    void add_existing(std::vector<std::string> &result, const std::string &path, std::vector<std::string> *warnings,
                      const std::string &kind) const {
        if (is_blank(path)) {
            return;
        }
        const std::string normalized = normalize(path);
        if (is_blank(normalized)) {
            add_warning(warnings, "invalid " + kind + " resource path: " + path);
        } else if (exists(normalized) && !contains(result, normalized)) {
            result.push_back(normalized);
        } else if (!exists(normalized)) {
            add_warning(warnings, "missing " + kind + " resource: " + normalized);
        }
    }

    void read_path_field(std::vector<std::string> &result, const json::Value &element,
                         std::vector<std::string> *warnings) const {
        if (!element.present()) {
            return;
        }
        if (element.is_primitive()) {
            add_existing(result, element.as_string(), warnings, "optional");
        } else if (element.is_array()) {
            for (const json::Value &child : element.items()) {
                read_path_field(result, child, warnings);
            }
        } else if (element.is_object()) {
            const std::string path = json::string_of(element, "path", json::string_of(element, "file", ""));
            if (!is_blank(path)) {
                add_existing(result, path, warnings, "optional");
                return;
            }
            for (const auto &member : element.members()) {
                if (member.second.is_primitive()) {
                    add_existing(result, member.second.as_string(), warnings, "optional");
                }
            }
        }
    }

    void add_resource_paths(std::vector<std::string> &result, const json::Value &element,
                            std::vector<std::string> *warnings, const std::string &kind) const {
        if (!element.present() || element.is_null()) {
            return;
        }
        if (element.is_primitive()) {
            add_existing(result, element.as_string(), warnings, kind);
            return;
        }
        if (element.is_array()) {
            for (const json::Value &child : element.items()) {
                add_resource_paths(result, child, warnings, kind);
            }
            return;
        }
        if (!element.is_object()) {
            return;
        }
        const std::string path = json::string_of(element, "path", json::string_of(element, "file", ""));
        if (!is_blank(path)) {
            add_existing(result, path, warnings, kind);
            return;
        }
        for (const auto &member : element.members()) {
            add_resource_paths(result, member.second, warnings, kind);
        }
    }

    void collect_sub_entity_paths(std::vector<std::string> &result, const json::Value &section,
                                  std::vector<std::string> *warnings, const std::string &kind) const {
        if (!section.present() || section.is_null()) {
            return;
        }
        if (section.is_array()) {
            for (const json::Value &element : section.items()) {
                collect_sub_entity_paths(result, element, warnings, kind);
            }
            return;
        }
        if (section.is_object()) {
            const std::string model = json::string_of(section, "model", json::string_of(section, "model_path", ""));
            if (!is_blank(model)) {
                add_existing(result, model, warnings, kind + " model");
                add_resource_paths(result, section.get("texture"), warnings, kind + " texture");
                add_resource_paths(result, section.get("textures"), warnings, kind + " texture");
                add_resource_paths(result, section.get("animation"), warnings, kind + " animation");
                add_resource_paths(result, section.get("animations"), warnings, kind + " animation");
                add_resource_paths(result, section.get("controller"), warnings, kind + " controller");
                add_resource_paths(result, section.get("animation_controller"), warnings, kind + " controller");
                add_resource_paths(result, section.get("controller_path"), warnings, kind + " controller");
                return;
            }
            for (const auto &member : section.members()) {
                collect_sub_entity_paths(result, member.second, warnings, kind);
            }
            return;
        }
        add_resource_paths(result, section, warnings, kind + " model");
    }

    void collect_directory_files_into(std::vector<std::string> &result,
                                      std::initializer_list<const char *> directories) const {
        for (const std::string &relative : paths_) {
            const std::string normalized = normalize(relative);
            const std::string lower = lower_ascii(normalized);
            for (const char *directory : directories) {
                const std::string prefix = lower_ascii(normalize(directory));
                if (starts_with(lower, prefix) && !contains(result, normalized)) {
                    result.push_back(normalized);
                    break;
                }
            }
        }
    }

    void collect_directory_json(std::vector<std::string> &result, const char *directory) const {
        const std::string prefix = normalize(directory);
        for (const std::string &relative : paths_) {
            const std::string normalized = normalize(relative);
            if (starts_with(normalized, prefix) && ends_with(lower_ascii(normalized), ".json") &&
                !contains(result, normalized)) {
                result.push_back(normalized);
            }
        }
    }

    void collect_animation_files(std::vector<std::string> &result) const {
        for (const std::string &relative : paths_) {
            const std::string normalized = normalize(relative);
            if (ends_with(lower_ascii(normalized), ".animation.json") && !contains(result, normalized)) {
                result.push_back(normalized);
            }
        }
    }

    void collect_animation_controller_files(std::vector<std::string> &result) const {
        for (const std::string &relative : paths_) {
            const std::string normalized = normalize(relative);
            const std::string lower = lower_ascii(normalized);
            if (!ends_with(lower, ".json") || contains(result, normalized) || ends_with(lower, ".animation.json")) {
                continue;
            }
            if (lower.find("animation_controller") != std::string::npos ||
                lower.find("animation.controllers") != std::string::npos ||
                ends_with(lower, "controller.animation.json")) {
                result.push_back(normalized);
            }
        }
    }

    std::string read_texture_channel(const json::Value &texture, const char *key,
                                     std::vector<std::string> *warnings) const {
        const std::string configured = json::string_of(texture, key, "");
        if (is_blank(configured)) {
            return std::string();
        }
        const std::string path = normalize(configured);
        if (is_blank(path) || !exists(path)) {
            add_warning(warnings, std::string("missing optional ") + key + " texture: " + display_path(configured));
            return std::string();
        }
        return path;
    }

    void add_new_texture(std::vector<TextureOption> &result, const json::Value &element,
                         std::vector<std::string> *warnings) const {
        if (!element.present() || element.is_null()) {
            return;
        }
        if (element.is_primitive()) {
            const std::string path = element.as_string();
            const std::string normalized = normalize(path);
            result.push_back(TextureOption{id_from_path(path), display_from_path(path), normalized, "", ""});
            if (is_blank(normalized)) {
                add_warning(warnings, "invalid optional texture: " + display_path(path));
            } else if (!exists(normalized)) {
                add_warning(warnings, "missing optional texture: " + normalized);
            }
            return;
        }
        if (!element.is_object()) {
            return;
        }
        std::string path = json::string_of(element, "path", json::string_of(element, "uv", ""));
        if (is_blank(path)) {
            for (const auto &member : element.members()) {
                if (member.second.is_primitive()) {
                    path = member.second.as_string();
                    break;
                }
            }
        }
        if (is_blank(path)) {
            return;
        }
        TextureOption option;
        option.id = json::string_of(element, "id", id_from_path(path));
        option.name = json::string_of(element, "name", display_from_path(path));
        option.path = normalize(path);
        option.normal_path = read_texture_channel(element, "normal", warnings);
        option.specular_path = read_texture_channel(element, "specular", warnings);
        result.push_back(option);
        if (is_blank(option.path)) {
            add_warning(warnings, "invalid optional texture: " + display_path(path));
        } else if (!exists(option.path)) {
            add_warning(warnings, "missing optional texture: " + option.path);
        }
    }

    std::vector<std::string> read_authors(const json::Value &object) const {
        std::vector<std::string> authors;
        const json::Value element = object.get("authors");
        if (element.is_array()) {
            for (const json::Value &author : element.items()) {
                std::string name;
                if (author.is_object()) {
                    name = json::string_of(author, "name", "");
                } else if (author.is_primitive()) {
                    name = author.as_string();
                }
                if (!is_blank(name)) {
                    authors.push_back(name);
                }
            }
        }
        const std::string author = json::string_of(object, "author", "");
        if (authors.empty() && !is_blank(author)) {
            authors.push_back(author);
        }
        return authors;
    }

    std::string first_existing(std::initializer_list<const char *> paths) const {
        for (const char *path : paths) {
            if (exists(path)) {
                return normalize(path);
            }
        }
        return std::string();
    }

    void build_index(Manifest &manifest, const json::Value &manifest_root) const {
        std::vector<std::string> *warnings = &manifest.warnings;
        collect_directory_files_into(manifest.languages, {"lang/", "language/"});
        if (manifest_root.present()) {
            const json::Value files = json::obj(manifest_root, "files");
            add_resource_paths(manifest.languages, files.get("language"), warnings, "language");
            add_resource_paths(manifest.languages, files.get("lang"), warnings, "language");
            collect_sub_entity_paths(manifest.projectiles, files.get("projectiles"), warnings, "projectile");
            collect_sub_entity_paths(manifest.vehicles, files.get("vehicles"), warnings, "vehicle");
        }
        collect_directory_files_into(manifest.projectiles, {"projectile/", "projectiles/"});
        collect_directory_files_into(manifest.vehicles, {"vehicle/", "vehicles/"});
        for (const std::string &relative : paths_) {
            const std::string normalized = normalize(relative);
            if (starts_with(normalized, "functions/") && ends_with(lower_ascii(normalized), ".molang")) {
                manifest.functions.push_back(normalized);
            }
            if (ends_with(lower_ascii(relative), ".ogg")) {
                manifest.sounds.push_back(relative);
            }
        }
        manifest.avatar_image = first_existing({"avatar.png", "icon.png", "textures/avatar.png", "textures/icon.png"});
        manifest.background_image = first_existing({"background.png", "textures/background.png", "gui/background.png"});
        if (!exists(manifest.main_model)) {
            manifest.main_model.clear();
        }
        manifest.arm_model = existing_or_blank(manifest.arm_model);
    }

    Manifest read_new(std::string &out_root_json) {
        const json::Value root = parse_object("ysm.json");
        out_root_json = root.present() ? root.to_json() : "{}";
        const json::Value metadata = json::obj(root, "metadata");
        const json::Value properties = json::obj(root, "properties");
        const json::Value player = json::obj(json::obj(root, "files"), "player");
        const json::Value model = json::obj(player, "model");

        Manifest manifest;
        manifest.kind = AvatarKind::current;
        manifest.name = json::string_of(metadata, "name", display_name_);
        manifest.description = json::string_of(metadata, "tips", json::string_of(metadata, "description", ""));
        const std::string configured_main = json::string_of(model, "main", "models/main.json");
        manifest.main_model = existing_or_blank(configured_main);
        if (is_blank(manifest.main_model)) {
            manifest.warnings.push_back("missing required resource: " + display_path(configured_main));
        }
        const std::string configured_arm = json::string_of(model, "arm", "");
        manifest.arm_model = existing_or_blank(configured_arm);
        if (!is_blank(configured_arm) && is_blank(manifest.arm_model)) {
            manifest.warnings.push_back("missing optional resource: " + display_path(configured_arm));
        }

        const json::Value textures = player.get("texture");
        if (textures.is_array()) {
            for (const json::Value &texture : textures.items()) {
                add_new_texture(manifest.textures, texture, &manifest.warnings);
            }
        } else if (textures.present()) {
            add_new_texture(manifest.textures, textures, &manifest.warnings);
        }
        manifest.default_texture = json::string_of(properties, "default_texture", "");

        read_path_field(manifest.animations, player.get("animation"), &manifest.warnings);
        read_path_field(manifest.animations, player.get("extra_animation"), &manifest.warnings);
        read_path_field(manifest.animations, player.get("animation_extra"), &manifest.warnings);
        read_path_field(manifest.controllers, player.get("animation_controllers"), &manifest.warnings);
        read_path_field(manifest.controllers, player.get("animation_controller"), &manifest.warnings);

        // firstPersonAnimationPaths (warnings discarded, as in the reference).
        std::vector<std::string> ignored;
        const json::Value animation = json::obj(player, "animation");
        read_path_field(manifest.first_person_animations, animation.get("fp_arm"), &ignored);
        read_path_field(manifest.first_person_animations, animation.get("fp.arm"), &ignored);

        build_index(manifest, root);
        manifest.authors = read_authors(metadata);
        manifest.free = json::bool_of(properties, "free", false);
        manifest.merge_multiline_expressions =
            lower_ascii(json::string_of(properties, "merge_multiline_expr", "false")) == "true";
        return manifest;
    }

    Manifest read_old(std::string &out_root_json) {
        const json::Value root = parse_object("main.json");
        json::Value description;
        const json::Value geometries = root.get("minecraft:geometry");
        if (geometries.is_array()) {
            const auto items = geometries.items();
            if (!items.empty() && items[0].is_object()) {
                description = json::obj(items[0], "description");
            }
        }
        const json::Value extra = json::obj(description, "ysm_extra_info");
        out_root_json = extra.present() ? extra.to_json() : "{}";

        Manifest manifest;
        manifest.kind = AvatarKind::legacy;
        manifest.name = json::string_of(extra, "name", display_name_);
        manifest.description = json::string_of(extra, "tips", "");
        manifest.main_model = existing_or_blank("main.json");
        if (is_blank(manifest.main_model)) {
            manifest.warnings.push_back("missing required resource: main.json");
        }
        manifest.arm_model = exists("arm.json") ? "arm.json" : "";

        for (const std::string &relative : paths_) {
            if (relative.find('/') != std::string::npos) {
                continue;
            }
            const std::string name = display_from_path(relative);
            const std::string lower = lower_ascii(name);
            if (ends_with(lower, ".png") && lower.find("foreground") == std::string::npos &&
                lower.find("background") == std::string::npos) {
                manifest.textures.push_back(
                    TextureOption{id_from_path(name), display_from_path(name), relative, "", ""});
            }
        }

        for (const char *path :
             {"main.animation.json", "arm.animation.json", "extra.animation.json", "tac.animation.json",
              "carryon.animation.json", "slashblade.animation.json", "tlm.animation.json"}) {
            add_existing(manifest.animations, path, nullptr, "optional");
        }
        collect_animation_files(manifest.animations);

        add_existing(manifest.controllers, "controller.animation.json", nullptr, "optional");
        add_existing(manifest.controllers, "animation_controllers.json", nullptr, "optional");
        collect_directory_json(manifest.controllers, "controller/");
        collect_directory_json(manifest.controllers, "controllers/");
        collect_animation_controller_files(manifest.controllers);

        build_index(manifest, json::Value());
        manifest.authors = read_authors(extra);
        manifest.free = json::bool_of(extra, "free", false);
        return manifest;
    }

    const ResourceSet &resources_;
    std::string display_name_;
    std::vector<std::string> paths_;
    std::vector<json::Document> documents_;
};

} // namespace

std::string display_from_path(const std::string &path) {
    if (is_blank(path)) {
        return "texture";
    }
    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    while (!normalized.empty() && normalized.back() == '/') {
        normalized.pop_back();
    }
    const size_t slash = normalized.rfind('/');
    return slash == std::string::npos ? normalized : normalized.substr(slash + 1);
}

std::string id_from_path(const std::string &path) {
    const std::string name = display_from_path(path);
    const size_t dot = name.rfind('.');
    return dot != std::string::npos && dot > 0 ? name.substr(0, dot) : name;
}

Manifest read_manifest(const ResourceSet &resources, const std::string &display_name, std::string &out_root_json) {
    Reader reader(resources, display_name);
    return reader.read(out_root_json);
}

std::optional<json::Document> parse_resource_json(const ResourceSet &resources, const std::string &path,
                                                  std::vector<std::string> &diagnostics) {
    const Resource *resource = resources.find(path);
    if (resource == nullptr) {
        diagnostics.push_back("missing resource: " + path);
        return std::nullopt;
    }
    std::string error;
    auto document = json::Document::parse(json::decode_utf8_lenient(resource->bytes), &error);
    if (!document) {
        diagnostics.push_back(path + ": invalid JSON: " + error);
    }
    return document;
}

} // namespace omm::ysm
