// Port of OMP YsmBinaryReader / YsmBinaryInput (format-dispatched, read-only).
// Field order, limits and error stages follow the Java reference exactly; every
// structural surprise the reference reports as RuntimeException becomes INVALID_DATA
// at stage "binary/structure".
#include <cmath>
#include <cstring>
#include <limits>
#include <span>

#include "ysmparser/binary_model.hpp"

namespace omm::ysm::binary {

namespace {

// Raised where the Java reference throws an unchecked exception.
struct StructureError {};

bool is_blank(const std::string &text) {
    for (char c : text) {
        if (!(c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v')) {
            return false;
        }
    }
    return true;
}

std::string animation_key(int32_t type) {
    switch (type) {
    case 1:
        return "main";
    case 2:
        return "arm";
    case 3:
        return "extra";
    case 4:
        return "tac";
    case 5:
        return "arrow";
    case 6:
        return "carryon";
    case 7:
        return "parcool";
    case 8:
        return "swem";
    case 9:
        return "slashblade";
    case 10:
        return "tlm";
    case 11:
        return "fp_arm";
    case 12:
        return "immersive_melodies";
    case 13:
        return "irons_spell_books";
    default:
        return "type_" + std::to_string(type);
    }
}

class Input {
public:
    Input(std::span<const uint8_t> data, Budget &budget) : reader_(data, budget, "binary"), budget_(budget) {
        budget.length(static_cast<int64_t>(data.size()), budget.limits().total_resource_bytes, "binary/input", 0);
    }

    uint64_t offset() const noexcept {
        return reader_.offset();
    }

    uint64_t remaining() const noexcept {
        return reader_.remaining();
    }

    int32_t dword() {
        return reader_.i32(Endian::little);
    }

    int32_t var_int() {
        return reader_.var_int();
    }

    int64_t var_long() {
        return reader_.var_long();
    }

    float f32() {
        return reader_.f32_finite();
    }

    uint8_t byte() {
        return reader_.u8();
    }

    std::string string() {
        return reader_.string();
    }

    void skip(uint64_t count) {
        reader_.skip(count);
    }

    uint32_t count(uint64_t max) {
        const int32_t value = reader_.var_int();
        return budget_.length(value, max, "binary/count", reader_.offset());
    }

    uint32_t geometry_bones_count() {
        const uint32_t value = count(4096 - geometry_bones_);
        geometry_bones_ += value;
        return value;
    }

    uint32_t geometry_faces_count() {
        const uint32_t value = count(262144 - geometry_faces_);
        geometry_faces_ += value;
        return value;
    }

    uint32_t animation_keyframes_count() {
        const uint32_t value = count(1048576 - animation_keyframes_);
        animation_keyframes_ += value;
        return value;
    }

    void check_image_dimensions(int32_t width, int32_t height) {
        if (width <= 0 || height <= 0) {
            fail(DecodeCode::invalid_data, "binary/image-dimensions", reader_.offset());
        }
        budget_.length(static_cast<int64_t>(width) * height, Budget::kMaxTexturePixels, "binary/image-pixels",
                       reader_.offset());
    }

    float animation_length() {
        const float value = reader_.f32_raw();
        // The reference packer uses +infinity for an omitted animation_length.
        if (std::isnan(value) || value < 0.0f) {
            fail(DecodeCode::invalid_data, "binary/animation-length", reader_.offset() - 4);
        }
        return value;
    }

    std::vector<uint8_t> byte_array() {
        budget_.begin_resource("binary/resource-count", reader_.offset());
        const int32_t declared = reader_.var_int();
        const uint32_t length =
            budget_.length(declared, budget_.remaining_resource_bytes(), "binary/resource", reader_.offset());
        reader_.require(length);
        budget_.resource(length, "binary/resource", reader_.offset());
        const auto bytes = reader_.bytes(length);
        return std::vector<uint8_t>(bytes.begin(), bytes.end());
    }

private:
    ByteReader reader_;
    Budget &budget_;
    uint32_t geometry_bones_ = 0;
    uint32_t geometry_faces_ = 0;
    uint32_t animation_keyframes_ = 0;
};

class Reader {
public:
    Reader(std::span<const uint8_t> data, Budget &budget) : input_(data, budget) {
        format_ = input_.dword();
        if (format_ < 1 || format_ > 32) {
            fail(DecodeCode::unsupported_version, "binary/format", 0);
        }
        model_.format_version = format_;
    }

    Model read() {
        try {
            if (format_ < 4) {
                deserialize_legacy_v1();
            } else if (format_ <= 15) {
                deserialize_legacy_v15();
            } else {
                deserialize_modern();
            }
            if (input_.remaining() > 0 && format_ >= 9) {
                read_footer();
            }
            if (input_.remaining() != 0) {
                fail(DecodeCode::invalid_data, "binary/trailing-data", input_.offset());
            }
        } catch (const StructureError &) {
            fail(DecodeCode::invalid_data, "binary/structure", input_.offset());
        } catch (const std::out_of_range &) {
            fail(DecodeCode::invalid_data, "binary/structure", input_.offset());
        }
        return std::move(model_);
    }

private:
    template <typename V>
    void put_unique(OrderedMap<V> &target, const std::string &name, V value) {
        if (is_blank(name) || target.contains(name)) {
            fail(DecodeCode::invalid_data, "binary/duplicate-or-empty-name", input_.offset());
        }
        target.put(name, std::move(value));
    }

    // computeIfAbsent("minecraft:arrow") as used by arrow(): sets matchIds on creation.
    SubEntity &arrow() {
        const std::string key = "minecraft:arrow";
        if (SubEntity *existing = model_.projectiles.find(key)) {
            return *existing;
        }
        SubEntity sub;
        sub.identifier = key;
        sub.match_ids = {key};
        return model_.projectiles.put(key, std::move(sub));
    }

    // The animation-id-5 path creates the arrow entity without matchIds.
    SubEntity &arrow_without_match_ids() {
        const std::string key = "minecraft:arrow";
        if (SubEntity *existing = model_.projectiles.find(key)) {
            return *existing;
        }
        SubEntity sub;
        sub.identifier = key;
        return model_.projectiles.put(key, std::move(sub));
    }

    Vec3 vec3() {
        Vec3 value;
        value[0] = input_.f32();
        value[1] = input_.f32();
        value[2] = input_.f32();
        return value;
    }

    void read_footer() {
        model_.footer.version = format_ > 26 ? input_.var_int() : format_;
        model_.footer.unk_int1 = input_.var_int();
        model_.footer.rand = input_.string();
        model_.footer.time = input_.var_long();
        model_.footer.extra = input_.string();
        if (format_ >= 24) {
            model_.footer.unk_int2 = input_.var_int();
        }
    }

    void assign_main_models(std::vector<Geometry> &models) {
        for (Geometry &geometry : models) {
            switch (geometry.model_type) {
            case 1:
                model_.main_entity.main_model = std::move(geometry);
                break;
            case 2:
                model_.main_entity.arm_model = std::move(geometry);
                break;
            case 3:
                arrow().model = std::move(geometry);
                break;
            default:
                throw StructureError{};
            }
        }
    }

    // Shared by legacy V1 and V15 headers: model blobs (assigned immediately, as in the
    // reference, so arrow() runs before any animation-id-5 entry) and animation blobs.
    void read_legacy_models_and_animations(std::vector<std::pair<int32_t, std::string>> &anim_keys) {
        const int32_t skip_bytes = input_.var_int();
        if (skip_bytes < 0) {
            fail(DecodeCode::truncated, "binary", input_.offset());
        }
        input_.skip(static_cast<uint64_t>(skip_bytes));

        std::vector<Geometry> temp_models;
        const uint32_t model_count = input_.count(4096);
        for (uint32_t index = 0; index < model_count; ++index) {
            const int32_t model_id = input_.var_int();
            if (input_.var_int() != 1) {
                throw StructureError{};
            }
            Geometry geometry = parse_models();
            geometry.model_type = model_id;
            temp_models.push_back(std::move(geometry));
        }
        assign_main_models(temp_models);

        const uint32_t animation_count = input_.count(4096);
        for (uint32_t index = 0; index < animation_count; ++index) {
            const int32_t animation_id = input_.var_int();
            if (input_.var_int() != 1) {
                throw StructureError{};
            }
            AnimationFile file = parse_animations();
            file.anim_type = animation_id;
            const std::string key = animation_key(animation_id);
            if (animation_id == 5) {
                arrow_without_match_ids().animation_files.put(key, std::move(file));
            } else {
                put_unique(model_.main_entity.animation_files, key, std::move(file));
            }
            anim_keys.emplace_back(animation_id, key);
        }
    }

    // tempAnims.get(id): last file registered for the id, wherever it was stored.
    AnimationFile &legacy_animation(const std::vector<std::pair<int32_t, std::string>> &anim_keys, int32_t id) {
        for (auto it = anim_keys.rbegin(); it != anim_keys.rend(); ++it) {
            if (it->first != id) {
                continue;
            }
            AnimationFile *file = id == 5 ? arrow_without_match_ids().animation_files.find(it->second)
                                          : model_.main_entity.animation_files.find(it->second);
            if (file != nullptr) {
                return *file;
            }
        }
        throw StructureError{}; // NullPointerException in the reference.
    }

    // The reference writes the hash into every temp model of that type; only the last one of
    // each type survives in its slot, so writing the slot is equivalent.
    void back_fill_model_hashes() {
        const uint32_t table_size = input_.count(4096);
        for (uint32_t index = 0; index < table_size; ++index) {
            const int32_t model_id = input_.var_int();
            const std::string hash = input_.string();
            std::optional<Geometry> *slot = nullptr;
            if (model_id == 1) {
                slot = &model_.main_entity.main_model;
            } else if (model_id == 2) {
                slot = &model_.main_entity.arm_model;
            } else if (model_id == 3) {
                if (SubEntity *arrow_entity = model_.projectiles.find("minecraft:arrow")) {
                    slot = &arrow_entity->model;
                }
            }
            if (slot != nullptr && slot->has_value() && (*slot)->model_type == model_id) {
                (*slot)->sha256 = hash;
            }
        }
    }

    void deserialize_legacy_v1() {
        std::vector<std::pair<int32_t, std::string>> anim_keys;
        read_legacy_models_and_animations(anim_keys);

        const uint32_t texture_count = input_.count(4096);
        for (uint32_t index = 0; index < texture_count; ++index) {
            Texture texture;
            texture.name = input_.string();
            if (format_ < 4) {
                if (input_.var_int() != 0x01) {
                    throw StructureError{};
                }
            }
            texture.data = input_.byte_array();
            texture.width = input_.var_int();
            texture.height = input_.var_int();
            input_.check_image_dimensions(texture.width, texture.height);
            texture.image_format = -1;
            const std::string name = texture.name;
            if (name == "arrow.png") {
                arrow().textures.put(name, std::move(texture));
            } else {
                put_unique(model_.main_entity.textures, name, std::move(texture));
            }
        }

        back_fill_model_hashes();

        const uint32_t animation_table = input_.count(4096);
        for (uint32_t index = 0; index < animation_table; ++index) {
            const int32_t animation_id = input_.var_int();
            AnimationFile &file = legacy_animation(anim_keys, animation_id);
            file.file_hash = input_.string();
        }

        const uint32_t texture_table = input_.count(4096);
        for (uint32_t index = 0; index < texture_table; ++index) {
            const std::string name = input_.string();
            const std::string hash = input_.string();
            if (Texture *texture = model_.main_entity.textures.find(name)) {
                texture->hash = hash;
            }
        }
        model_.properties.sha256 = input_.string();
    }

    void deserialize_legacy_v15() {
        std::vector<std::pair<int32_t, std::string>> anim_keys;
        read_legacy_models_and_animations(anim_keys);

        if (format_ > 9) {
            parse_animation_controllers(model_.main_entity.animation_controller_files, false);
            const uint32_t table = input_.count(4096);
            for (uint32_t index = 0; index < table; ++index) {
                const std::string name = input_.string();
                const std::string hash = input_.string();
                for (ControllerFile &file : model_.main_entity.animation_controller_files) {
                    if (name == file.name) {
                        file.hash = hash;
                        break;
                    }
                }
            }
        }

        const uint32_t texture_count = input_.count(4096);
        for (uint32_t index = 0; index < texture_count; ++index) {
            Texture texture;
            texture.name = input_.string();
            texture.data = input_.byte_array();
            texture.width = input_.var_int();
            texture.height = input_.var_int();
            input_.check_image_dimensions(texture.width, texture.height);
            texture.image_format = -1;
            const uint32_t sub_count = input_.count(4096);
            for (uint32_t sub_index = 0; sub_index < sub_count; ++sub_index) {
                SubTexture sub;
                sub.specular_type = input_.var_int();
                sub.data = input_.byte_array();
                sub.width = input_.var_int();
                sub.height = input_.var_int();
                input_.check_image_dimensions(sub.width, sub.height);
                sub.image_format = -1;
                texture.sub_textures.push_back(std::move(sub));
            }
            const std::string name = texture.name;
            if (name == "/ARROW\\") {
                arrow().textures.put(name, std::move(texture));
            } else {
                put_unique(model_.main_entity.textures, name, std::move(texture));
            }
        }

        if (format_ > 9) {
            parse_sound_files();
            const uint32_t table = input_.count(4096);
            for (uint32_t index = 0; index < table; ++index) {
                const std::string name = input_.string();
                const std::string hash = input_.string();
                if (DataFile *sound = model_.sound_files.find(name)) {
                    sound->hash = hash;
                }
            }
        }

        std::vector<size_t> temp_avatars;
        const uint32_t extra_count = input_.count(4096);
        for (uint32_t index = 0; index < extra_count; ++index) {
            Image avatar;
            avatar.name = input_.string();
            avatar.data = input_.byte_array();
            avatar.width = input_.var_int();
            avatar.height = input_.var_int();
            input_.check_image_dimensions(avatar.width, avatar.height);
            avatar.format = -1;
            temp_avatars.push_back(model_.images.size());
            model_.images.push_back(std::move(avatar));
        }

        back_fill_model_hashes();

        const uint32_t animation_table = input_.count(4096);
        for (uint32_t index = 0; index < animation_table; ++index) {
            const int32_t animation_id = input_.var_int();
            const std::string hash = input_.string();
            legacy_animation(anim_keys, animation_id).file_hash = hash;
        }

        const uint32_t texture_table = input_.count(4096);
        for (uint32_t index = 0; index < texture_table; ++index) {
            const std::string name = input_.string();
            const std::string hash = input_.string();
            Texture *texture = model_.main_entity.textures.find(name);
            if (texture != nullptr) {
                texture->hash = hash;
            }
            const uint32_t sub_count = input_.count(4096);
            for (uint32_t sub_index = 0; sub_index < sub_count; ++sub_index) {
                const int32_t specular_type = input_.var_int();
                const std::string sub_hash = input_.string();
                if (texture != nullptr) {
                    for (SubTexture &sub : texture->sub_textures) {
                        if (sub.specular_type == specular_type) {
                            sub.hash = sub_hash;
                            break;
                        }
                    }
                }
            }
        }
        for (auto &projectile : model_.projectiles.entries()) {
            for (auto &texture : projectile.second.textures.entries()) {
                model_.main_entity.textures.erase(texture.second.name);
            }
        }

        parse_ysm_json();

        for (size_t index = 0; index < temp_avatars.size(); ++index) {
            assign_avatar(index, temp_avatars[index]);
        }
    }

    void assign_avatar(size_t author_index, size_t image_index) {
        if (author_index < model_.metadata.authors.size()) {
            model_.metadata.authors[author_index].avatar = model_.images[image_index].name;
            model_.metadata.authors[author_index].avatar_image = image_index;
        } else {
            model_.metadata.extra_avatars.push_back(image_index);
        }
    }

    void deserialize_modern() {
        parse_sound_files();
        parse_function_files();
        parse_language_files();

        if (format_ < 26) {
            const uint32_t total = input_.count(4096);
            for (uint32_t index = 0; index < total; ++index) {
                parse_sub_entity(model_.vehicles, "SubEntity", static_cast<int32_t>(index));
            }
            input_.var_int(); // footer flag, always 0
        } else {
            const uint32_t vehicles = input_.count(4096);
            for (uint32_t index = 0; index < vehicles; ++index) {
                parse_sub_entity(model_.vehicles, "Vehicle", static_cast<int32_t>(index));
            }
            const uint32_t projectiles = input_.count(4096);
            for (uint32_t index = 0; index < projectiles; ++index) {
                parse_sub_entity(model_.projectiles, "Projectile", static_cast<int32_t>(index));
            }
        }

        if (input_.var_int() != 1) {
            fail(DecodeCode::invalid_data, "binary/entity-flag", input_.offset());
        }

        const uint32_t animation_count = input_.count(4096);
        for (uint32_t index = 0; index < animation_count; ++index) {
            const int32_t type = input_.var_int();
            const std::string hash = input_.string();
            AnimationFile file = parse_animations();
            file.anim_type = type;
            file.file_hash = hash;
            put_unique(model_.main_entity.animation_files, animation_key(type), std::move(file));
        }

        parse_animation_controllers(model_.main_entity.animation_controller_files, true);
        parse_texture_files(model_.main_entity.textures);

        const uint32_t model_count = input_.count(4096);
        std::vector<Geometry> temp_models;
        for (uint32_t index = 0; index < model_count; ++index) {
            const int32_t model_type = input_.var_int();
            const std::string hash = input_.string();
            Geometry geometry = parse_models();
            geometry.sha256 = hash;
            geometry.model_type = model_type;
            temp_models.push_back(std::move(geometry));
        }
        assign_main_models(temp_models);

        parse_ysm_json();
    }

    void parse_sub_entity(OrderedMap<SubEntity> &target, const std::string &category, int32_t index) {
        SubEntity sub;
        if (format_ <= 26) {
            sub.identifier = input_.string();
        } else {
            sub.identifier = category + "_" + std::to_string(index);
        }
        const uint32_t animation_count = input_.count(4096);
        for (uint32_t animation = 0; animation < animation_count; ++animation) {
            const std::string hash = input_.string();
            AnimationFile file = parse_animations();
            file.file_hash = hash;
            put_unique(sub.animation_files, category + "_" + std::to_string(animation), std::move(file));
        }

        parse_animation_controllers(sub.animation_controller_files, false);

        Texture base;
        base.hash = input_.string();
        base.data = input_.byte_array();
        base.width = input_.var_int();
        base.height = input_.var_int();
        input_.check_image_dimensions(base.width, base.height);
        base.image_format = input_.var_int();
        base.unknown_flag = input_.var_int();
        base.name = "base_texture_" + std::to_string(index);

        const uint32_t sub_count = input_.count(4096);
        for (uint32_t sub_index = 0; sub_index < sub_count; ++sub_index) {
            SubTexture texture;
            texture.specular_type = input_.var_int();
            texture.hash = input_.string();
            texture.data = input_.byte_array();
            texture.width = input_.var_int();
            texture.height = input_.var_int();
            input_.check_image_dimensions(texture.width, texture.height);
            texture.image_format = input_.var_int();
            texture.unknown_flag = input_.var_int();
            base.sub_textures.push_back(std::move(texture));
        }
        const std::string base_name = base.name;
        sub.textures.put(base_name, std::move(base));

        const std::string model_hash = input_.string();
        sub.model = parse_models();
        sub.model->sha256 = model_hash;

        if (format_ > 26) {
            const uint32_t identifier_count = input_.count(128);
            if (identifier_count == 0) {
                fail(DecodeCode::invalid_data, "binary/subentity-identifiers", input_.offset());
            }
            std::vector<std::string> identifiers;
            for (uint32_t id = 0; id < identifier_count; ++id) {
                identifiers.push_back(input_.string());
            }
            sub.identifier = identifiers.front();
            sub.match_ids = std::move(identifiers);
        }
        const std::string identifier = sub.identifier;
        put_unique(target, identifier, std::move(sub));
    }

    Geometry parse_models() {
        Geometry geometry;
        const uint32_t bone_count = input_.geometry_bones_count();
        for (uint32_t bone_index = 0; bone_index < bone_count; ++bone_index) {
            Bone bone;
            bone.parent_name = input_.string();
            const uint32_t cube_count = input_.count(4096);
            for (uint32_t cube_index = 0; cube_index < cube_count; ++cube_index) {
                Cube cube;
                const uint32_t face_count = input_.geometry_faces_count();
                for (uint32_t face_index = 0; face_index < face_count; ++face_index) {
                    Face face;
                    face.normal = vec3();
                    for (int vertex = 0; vertex < 4; ++vertex) {
                        face.positions[vertex] = vec3();
                        face.u[vertex] = input_.f32();
                        face.v[vertex] = input_.f32();
                    }
                    cube.faces.push_back(face);
                }
                cube.unk_int1 = input_.var_int();
                cube.unk_int2 = input_.var_int();
                cube.unk_int3 = input_.var_int();
                bone.cubes.push_back(std::move(cube));
            }
            bone.name = input_.string();
            for (int32_t &pad : bone.unk_pad) {
                pad = input_.var_int();
            }
            bone.pivot = vec3();
            bone.rotation = vec3();
            geometry.bones.push_back(std::move(bone));
        }

        geometry.identifier = input_.string();
        geometry.texture_height = input_.f32();
        geometry.texture_width = input_.f32();
        geometry.visible_bounds_height = input_.f32();
        geometry.visible_bounds_width = input_.f32();
        const uint32_t offset_size = input_.count(3);
        for (uint32_t index = 0; index < offset_size; ++index) {
            geometry.visible_bounds_offset.push_back(input_.f32());
        }
        geometry.unk_float1 = input_.f32();
        geometry.unk_float2 = input_.f32();
        if (input_.var_int() > 0) {
            parse_legacy_ysm_info();
        }
        for (int32_t &pad : geometry.footer_pad) {
            pad = input_.var_int();
        }
        return geometry;
    }

    void parse_ysm_json() {
        model_.properties.sha256 = input_.string();
        const int32_t is_new_version = input_.var_int();

        if (is_new_version != 0) {
            if (format_ <= 15) {
                input_.var_int();
            }
            model_.metadata.name = input_.string();
            model_.metadata.tips = input_.string();
            model_.metadata.license_type = input_.string();
            model_.metadata.license_description = input_.string();

            const uint32_t authors = input_.count(4096);
            for (uint32_t index = 0; index < authors; ++index) {
                Author author;
                author.name = input_.string();
                author.role = input_.string();
                const uint32_t contacts = input_.count(4096);
                for (uint32_t contact = 0; contact < contacts; ++contact) {
                    std::string key = input_.string();
                    author.contacts.put(key, input_.string());
                }
                author.comment = input_.string();
                model_.metadata.authors.push_back(std::move(author));
            }
            const uint32_t links = input_.count(4096);
            for (uint32_t index = 0; index < links; ++index) {
                std::string key = input_.string();
                model_.metadata.links.put(key, input_.string());
            }
        }

        if (is_new_version == 0 && format_ <= 15) {
            return;
        }

        Properties &properties = model_.properties;
        properties.width_scale = input_.f32();
        properties.height_scale = input_.f32();

        const uint32_t extra_animations = input_.count(4096);
        for (uint32_t index = 0; index < extra_animations; ++index) {
            std::string key = input_.string();
            put_unique(properties.extra_animations, key, input_.string());
        }

        if (format_ > 9) {
            const uint32_t buttons = input_.count(4096);
            for (uint32_t index = 0; index < buttons; ++index) {
                ExtraAnimationButton button;
                button.id = input_.string();
                button.name = input_.string();
                input_.var_int(); // button padding
                const uint32_t forms = input_.count(4096);
                for (uint32_t form_index = 0; form_index < forms; ++form_index) {
                    ConfigForm form;
                    form.type = input_.string();
                    form.title = input_.string();
                    form.description = input_.string();
                    form.default_value = input_.string();
                    form.step = input_.f32();
                    form.min = input_.f32();
                    form.max = input_.f32();
                    const uint32_t labels = input_.count(4096);
                    for (uint32_t label = 0; label < labels; ++label) {
                        std::string key = input_.string();
                        form.labels.put(key, input_.string());
                    }
                    button.forms.push_back(std::move(form));
                }
                properties.extra_animation_buttons.push_back(std::move(button));
            }

            const uint32_t classifies = input_.count(4096);
            for (uint32_t index = 0; index < classifies; ++index) {
                ExtraAnimationClassify classify;
                classify.id = input_.string();
                const uint32_t extras = input_.count(4096);
                for (uint32_t extra = 0; extra < extras; ++extra) {
                    std::string key = input_.string();
                    classify.extras.put(key, input_.string());
                }
                properties.extra_animation_classifies.push_back(std::move(classify));
            }
        }

        properties.default_texture = input_.string();
        properties.preview_animation = input_.string();
        properties.is_free = input_.var_int() != 0;
        if (format_ > 4) {
            properties.render_layers_first = input_.var_int() != 0;
        }
        if (format_ >= 15) {
            properties.all_cutout = input_.var_int() != 0;
            properties.disable_preview_rotation = input_.var_int() != 0;
        }
        if (format_ > 15) {
            properties.gui_no_lighting = input_.var_int() != 0;
            if (format_ >= 32) {
                properties.merge_multiline_expr = input_.var_int() != 0;
            }
            properties.gui_foreground = input_.string();
            properties.gui_background = input_.string();

            const uint32_t avatars = input_.count(4096);
            for (uint32_t index = 0; index < avatars; ++index) {
                model_.images.push_back(read_image());
                assign_avatar(index, model_.images.size() - 1);
            }
        }
        if (format_ <= 15) {
            return;
        }
        const uint32_t backgrounds = input_.count(4096);
        for (uint32_t index = 0; index < backgrounds; ++index) {
            model_.images.push_back(read_image());
            properties.background_images.push_back(model_.images.size() - 1);
        }
    }

    Image read_image() {
        Image image;
        image.name = input_.string();
        image.data = input_.byte_array();
        image.width = input_.var_int();
        image.height = input_.var_int();
        input_.check_image_dimensions(image.width, image.height);
        image.format = input_.var_int();
        image.unknown_flag = input_.var_int();
        return image;
    }

    void parse_legacy_ysm_info() {
        model_.metadata.name = input_.string();
        model_.metadata.tips = input_.string();
        const uint32_t extra_animations = input_.count(4096);
        for (uint32_t index = 0; index < extra_animations; ++index) {
            model_.properties.extra_animations.put("extra" + std::to_string(index), input_.string());
        }
        const uint32_t authors = input_.count(4096);
        for (uint32_t index = 0; index < authors; ++index) {
            Author author;
            author.name = input_.string();
            model_.metadata.authors.push_back(std::move(author));
        }
        model_.metadata.license_type = input_.string();
        model_.properties.is_free = input_.var_int() != 0;
    }

    Value tagged_value(const char *stage) {
        const uint8_t type = input_.byte();
        if (type == 0x01) {
            return input_.f32();
        }
        if (type == 0x02) {
            return input_.string();
        }
        fail(DecodeCode::invalid_data, stage, input_.offset());
    }

    AnimationFile parse_animations() {
        AnimationFile file;
        const uint32_t animation_count = input_.count(4096);
        for (uint32_t index = 0; index < animation_count; ++index) {
            Animation animation;
            animation.name = input_.string();
            animation.length = input_.animation_length() / 20.0f;
            animation.loop_mode = input_.var_int();

            if (format_ > 9) {
                animation.unk_int1 = input_.var_int();
                animation.unk_int2 = input_.var_int();
                const uint32_t blend_count = input_.count(4096);
                for (uint32_t blend = 0; blend < blend_count; ++blend) {
                    animation.blend_weight = tagged_value("binary/blend-value-type");
                }
                animation.unk_int4 = input_.var_int();
            }

            const uint32_t bone_count = input_.count(4096);
            for (uint32_t bone = 0; bone < bone_count; ++bone) {
                BoneAnimation bone_animation;
                bone_animation.bone_name = input_.string();
                parse_channel(bone_animation.rotation);
                parse_channel(bone_animation.position);
                parse_channel(bone_animation.scale);
                animation.bone_animations.push_back(std::move(bone_animation));
            }

            const uint32_t groups = input_.count(4096);
            for (uint32_t group = 0; group < groups; ++group) {
                TimelineEvent event;
                const uint32_t events = input_.count(4096);
                for (uint32_t item = 0; item < events; ++item) {
                    event.events.push_back(input_.string());
                }
                event.timestamp = input_.f32() / 20.0f;
                animation.timeline_events.push_back(std::move(event));
            }

            if (format_ > 9) {
                const uint32_t sounds = input_.count(4096);
                for (uint32_t sound = 0; sound < sounds; ++sound) {
                    SoundEffect effect;
                    effect.effect_name = input_.string();
                    effect.timestamp = input_.f32() / 20.0f;
                    animation.sound_effects.push_back(std::move(effect));
                }
            }
            const std::string name = animation.name;
            put_unique(file.animations, name, std::move(animation));
        }
        return file;
    }

    void parse_channel(std::vector<Keyframe> &channel) {
        const uint32_t keyframe_count = input_.animation_keyframes_count();
        for (uint32_t index = 0; index < keyframe_count; ++index) {
            Keyframe keyframe;
            keyframe.timestamp = input_.f32() / 20.0f;
            keyframe.interpolation_mode = input_.var_int();
            std::array<Value, 3> first;
            for (Value &value : first) {
                value = tagged_value("binary/keyframe-value-type");
            }
            keyframe.has_pre_data = input_.var_int() > 0;
            if (keyframe.has_pre_data) {
                for (Value &value : keyframe.post_data) {
                    value = tagged_value("binary/keyframe-value-type");
                }
                keyframe.pre_data = std::move(first);
            } else {
                keyframe.post_data = std::move(first);
            }
            channel.push_back(std::move(keyframe));
        }
    }

    void parse_animation_controllers(std::vector<ControllerFile> &target, bool read_name) {
        const uint32_t count = input_.count(4096);
        for (uint32_t index = 0; index < count; ++index) {
            ControllerFile file;
            if (format_ <= 15) {
                file.legacy_unknown_int = input_.var_int();
                file.name = "legacy_controller_" + std::to_string(index);
            } else {
                if (read_name) {
                    file.name = input_.string();
                }
                file.hash = input_.string();
            }
            parse_controller_body(file.controllers);
            target.push_back(std::move(file));
        }
    }

    void parse_controller_body(OrderedMap<Controller> &target) {
        const uint32_t count = input_.count(4096);
        for (uint32_t index = 0; index < count; ++index) {
            Controller controller;
            controller.animation_name = input_.string();
            controller.initial_state = input_.string();
            const uint32_t states = input_.count(4096);
            for (uint32_t state_index = 0; state_index < states; ++state_index) {
                ControllerState state;
                state.name = input_.string();
                const uint32_t animations = input_.count(4096);
                for (uint32_t item = 0; item < animations; ++item) {
                    std::string key = input_.string();
                    state.animations.put(key, input_.string());
                }
                const uint32_t transitions = input_.count(4096);
                for (uint32_t item = 0; item < transitions; ++item) {
                    std::string key = input_.string();
                    state.transitions.put(key, input_.string());
                }
                const uint32_t on_entry = input_.count(4096);
                for (uint32_t item = 0; item < on_entry; ++item) {
                    state.on_entry.push_back(input_.string());
                }
                const uint32_t on_exit = input_.count(4096);
                for (uint32_t item = 0; item < on_exit; ++item) {
                    state.on_exit.push_back(input_.string());
                }
                if (input_.var_int() != 0) {
                    state.blend_transition_value = input_.f32();
                } else {
                    const uint32_t points = input_.count(4096);
                    for (uint32_t item = 0; item < points; ++item) {
                        const float time = input_.f32();
                        const float value = input_.f32();
                        // Map.put with Float keys: Float.equals compares bit patterns.
                        bool replaced = false;
                        for (auto &point : state.blend_transitions) {
                            uint32_t a;
                            uint32_t b;
                            std::memcpy(&a, &point.first, 4);
                            std::memcpy(&b, &time, 4);
                            if (a == b) {
                                point.second = value;
                                replaced = true;
                                break;
                            }
                        }
                        if (!replaced) {
                            state.blend_transitions.emplace_back(time, value);
                        }
                    }
                }
                state.blend_via_shortest_path = input_.var_int() != 0;
                if (format_ > 26) {
                    const uint32_t sounds = input_.count(4096);
                    for (uint32_t item = 0; item < sounds; ++item) {
                        state.sound_effects.push_back(input_.string());
                    }
                }
                controller.states.push_back(std::move(state));
            }
            const std::string name = controller.animation_name;
            put_unique(target, name, std::move(controller));
        }
    }

    void parse_sound_files() {
        const uint32_t count = input_.count(4096);
        for (uint32_t index = 0; index < count; ++index) {
            const std::string name = input_.string();
            DataFile file;
            if (format_ > 15) {
                file.hash = input_.string();
            }
            file.data = input_.byte_array();
            put_unique(model_.sound_files, name, std::move(file));
        }
    }

    void parse_function_files() {
        const uint32_t count = input_.count(4096);
        for (uint32_t index = 0; index < count; ++index) {
            const std::string name = input_.string();
            DataFile file;
            file.hash = input_.string();
            file.data = input_.byte_array();
            put_unique(model_.function_files, name, std::move(file));
        }
    }

    void parse_language_files() {
        const uint32_t count = input_.count(4096);
        for (uint32_t index = 0; index < count; ++index) {
            const std::string name = input_.string();
            LanguageFile file;
            file.hash = input_.string();
            const uint32_t nodes = input_.count(4096);
            for (uint32_t node = 0; node < nodes; ++node) {
                std::string key = input_.string();
                file.data.put(key, input_.string());
            }
            put_unique(model_.language_files, name, std::move(file));
        }
    }

    void parse_texture_files(OrderedMap<Texture> &target) {
        const uint32_t count = input_.count(4096);
        for (uint32_t index = 0; index < count; ++index) {
            Texture texture;
            texture.name = input_.string();
            texture.hash = input_.string();
            texture.data = input_.byte_array();
            texture.width = input_.var_int();
            texture.height = input_.var_int();
            input_.check_image_dimensions(texture.width, texture.height);
            texture.image_format = input_.var_int();
            texture.unknown_flag = input_.var_int();
            const uint32_t sub_count = input_.count(4096);
            for (uint32_t sub_index = 0; sub_index < sub_count; ++sub_index) {
                SubTexture sub;
                sub.specular_type = input_.var_int();
                sub.hash = input_.string();
                sub.data = input_.byte_array();
                sub.width = input_.var_int();
                sub.height = input_.var_int();
                input_.check_image_dimensions(sub.width, sub.height);
                sub.image_format = input_.var_int();
                sub.unknown_flag = input_.var_int();
                texture.sub_textures.push_back(std::move(sub));
            }
            const std::string name = texture.name;
            put_unique(target, name, std::move(texture));
        }
    }

    Input input_;
    int32_t format_ = 0;
    Model model_;
};

} // namespace

Model read_payload(std::span<const uint8_t> payload, Budget &budget) {
    Reader reader(payload, budget);
    return reader.read();
}

} // namespace omm::ysm::binary
