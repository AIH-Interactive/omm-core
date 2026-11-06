// Canonical digest of a native binary::Model. Mirrors tools/m2/omp-harness/BinaryDigest.java
// token for token; the two SHA-256 values match only if every field matches.
#pragma once

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "sha256.hpp"
#include "ysmparser/binary_model.hpp"

namespace omm::test {

class BinaryDigest {
public:
    static std::string of(const ysm::binary::Model &model) {
        BinaryDigest writer;
        writer.model(model);
        return sha256_hex(
            std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(writer.stream_.data()), writer.stream_.size()));
    }

private:
    using Model = ysm::binary::Model;

    void raw(const std::string &text) {
        stream_ += text;
    }

    void s(const std::string &value) {
        raw("S" + std::to_string(value.size()) + ":");
        raw(value);
    }

    void s(const std::optional<std::string> &value) {
        if (!value) {
            raw("N");
        } else {
            s(*value);
        }
    }

    void i(long long value) {
        raw("I" + std::to_string(value) + ";");
    }

    void b(bool value) {
        i(value ? 1 : 0);
    }

    void f(float value) {
        uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        char text[16];
        std::snprintf(text, sizeof(text), "%x", bits);
        raw(std::string("F") + text + ";");
    }

    void bytes(const std::vector<uint8_t> &data) {
        raw("B" + sha256_hex(data) + ";");
    }

    void value(const ysm::binary::Value &item) {
        if (const float *number = std::get_if<float>(&item)) {
            raw("V");
            f(*number);
        } else {
            raw("T");
            s(std::get<std::string>(item));
        }
    }

    void map(const ysm::binary::OrderedMap<std::string> &values) {
        i(static_cast<long long>(values.size()));
        for (const auto &entry : values.entries()) {
            s(entry.first);
            s(entry.second);
        }
    }

    void list(const std::vector<std::string> &values) {
        i(static_cast<long long>(values.size()));
        for (const std::string &item : values) {
            s(item);
        }
    }

    void image(const Model &model, std::optional<size_t> index) {
        if (!index) {
            raw("N");
            return;
        }
        const ysm::binary::Image &image = model.images[*index];
        s(image.name);
        bytes(image.data);
        i(image.width);
        i(image.height);
        i(image.format);
        i(image.unknown_flag);
    }

    void model(const Model &model) {
        i(model.format_version);

        const auto &metadata = model.metadata;
        s(metadata.name);
        s(metadata.tips);
        s(metadata.license_type);
        s(metadata.license_description);
        i(static_cast<long long>(metadata.authors.size()));
        for (const auto &author : metadata.authors) {
            s(author.name);
            s(author.role);
            s(author.comment);
            map(author.contacts);
            s(author.avatar);
            image(model, author.avatar_image);
        }
        map(metadata.links);
        i(static_cast<long long>(metadata.extra_avatars.size()));
        for (size_t index : metadata.extra_avatars) {
            image(model, index);
        }

        const auto &properties = model.properties;
        s(properties.sha256);
        f(properties.width_scale);
        f(properties.height_scale);
        s(properties.default_texture);
        s(properties.preview_animation);
        b(properties.is_free);
        b(properties.render_layers_first);
        b(properties.all_cutout);
        b(properties.disable_preview_rotation);
        b(properties.gui_no_lighting);
        b(properties.merge_multiline_expr);
        s(properties.gui_foreground);
        s(properties.gui_background);
        i(static_cast<long long>(properties.background_images.size()));
        for (size_t index : properties.background_images) {
            image(model, index);
        }
        map(properties.extra_animations);
        i(static_cast<long long>(properties.extra_animation_classifies.size()));
        for (const auto &classify : properties.extra_animation_classifies) {
            s(classify.id);
            map(classify.extras);
        }
        i(static_cast<long long>(properties.extra_animation_buttons.size()));
        for (const auto &button : properties.extra_animation_buttons) {
            s(button.id);
            s(button.name);
            i(static_cast<long long>(button.forms.size()));
            for (const auto &form : button.forms) {
                s(form.type);
                s(form.title);
                s(form.description);
                s(form.default_value);
                f(form.step);
                f(form.min);
                f(form.max);
                map(form.labels);
            }
        }

        geometry(model.main_entity.main_model);
        geometry(model.main_entity.arm_model);
        textures(model.main_entity.textures);
        animation_files(model.main_entity.animation_files);
        controllers(model.main_entity.animation_controller_files);

        sub_entities(model.vehicles);
        sub_entities(model.projectiles);

        data_files(model.sound_files);
        data_files(model.function_files);
        i(static_cast<long long>(model.language_files.size()));
        for (const auto &entry : model.language_files.entries()) {
            s(entry.first);
            s(entry.second.hash);
            map(entry.second.data);
        }

        const auto &footer = model.footer;
        i(footer.version);
        i(footer.unk_int1);
        s(footer.rand);
        i(footer.time);
        s(footer.extra);
        i(footer.unk_int2);
    }

    void sub_entities(const ysm::binary::OrderedMap<ysm::binary::SubEntity> &entities) {
        i(static_cast<long long>(entities.size()));
        for (const auto &entry : entities.entries()) {
            s(entry.first);
            const auto &sub = entry.second;
            s(sub.identifier);
            if (sub.match_ids.empty()) {
                raw("N");
            } else {
                list(sub.match_ids);
            }
            geometry(sub.model);
            textures(sub.textures);
            animation_files(sub.animation_files);
            controllers(sub.animation_controller_files);
        }
    }

    void data_files(const ysm::binary::OrderedMap<ysm::binary::DataFile> &files) {
        i(static_cast<long long>(files.size()));
        for (const auto &entry : files.entries()) {
            s(entry.first);
            s(entry.second.hash);
            bytes(entry.second.data);
        }
    }

    void vec(const ysm::binary::Vec3 &values) {
        for (float item : values) {
            f(item);
        }
    }

    void geometry(const std::optional<ysm::binary::Geometry> &geometry) {
        if (!geometry) {
            raw("N");
            return;
        }
        i(geometry->model_type);
        s(geometry->identifier);
        s(geometry->sha256);
        f(geometry->texture_width);
        f(geometry->texture_height);
        f(geometry->visible_bounds_width);
        f(geometry->visible_bounds_height);
        i(static_cast<long long>(geometry->visible_bounds_offset.size()));
        for (float item : geometry->visible_bounds_offset) {
            f(item);
        }
        f(geometry->unk_float1);
        f(geometry->unk_float2);
        for (int32_t pad : geometry->footer_pad) {
            i(pad);
        }
        i(static_cast<long long>(geometry->bones.size()));
        for (const auto &bone : geometry->bones) {
            s(bone.name);
            s(bone.parent_name);
            vec(bone.pivot);
            vec(bone.rotation);
            for (int32_t pad : bone.unk_pad) {
                i(pad);
            }
            i(static_cast<long long>(bone.cubes.size()));
            for (const auto &cube : bone.cubes) {
                i(static_cast<long long>(cube.faces.size()));
                for (const auto &face : cube.faces) {
                    vec(face.normal);
                    for (int vertex = 0; vertex < 4; ++vertex) {
                        vec(face.positions[vertex]);
                        f(face.u[vertex]);
                        f(face.v[vertex]);
                    }
                }
                i(cube.unk_int1);
                i(cube.unk_int2);
                i(cube.unk_int3);
            }
        }
    }

    void textures(const ysm::binary::OrderedMap<ysm::binary::Texture> &textures) {
        i(static_cast<long long>(textures.size()));
        for (const auto &entry : textures.entries()) {
            s(entry.first);
            const auto &texture = entry.second;
            s(texture.name);
            s(texture.hash);
            i(texture.width);
            i(texture.height);
            i(texture.image_format);
            bytes(texture.data);
            i(texture.unknown_flag);
            i(static_cast<long long>(texture.sub_textures.size()));
            for (const auto &sub : texture.sub_textures) {
                s(sub.hash);
                i(sub.specular_type);
                i(sub.width);
                i(sub.height);
                i(sub.image_format);
                bytes(sub.data);
                i(sub.unknown_flag);
            }
        }
    }

    void channel(const std::vector<ysm::binary::Keyframe> &frames) {
        i(static_cast<long long>(frames.size()));
        for (const auto &frame : frames) {
            f(frame.timestamp);
            i(frame.interpolation_mode);
            for (const auto &item : frame.post_data) {
                value(item);
            }
            b(frame.has_pre_data);
            if (frame.has_pre_data) {
                for (const auto &item : frame.pre_data) {
                    value(item);
                }
            }
        }
    }

    void animation_files(const ysm::binary::OrderedMap<ysm::binary::AnimationFile> &files) {
        i(static_cast<long long>(files.size()));
        for (const auto &entry : files.entries()) {
            s(entry.first);
            const auto &file = entry.second;
            i(file.anim_type);
            s(file.file_hash);
            i(static_cast<long long>(file.animations.size()));
            for (const auto &clip_entry : file.animations.entries()) {
                s(clip_entry.first);
                const auto &clip = clip_entry.second;
                s(clip.name);
                f(clip.length);
                i(clip.loop_mode);
                if (!clip.blend_weight) {
                    raw("N");
                } else {
                    value(*clip.blend_weight);
                }
                i(clip.unk_int1);
                i(clip.unk_int2);
                i(clip.unk_int4);
                i(static_cast<long long>(clip.bone_animations.size()));
                for (const auto &bone : clip.bone_animations) {
                    s(bone.bone_name);
                    channel(bone.rotation);
                    channel(bone.position);
                    channel(bone.scale);
                }
                i(static_cast<long long>(clip.timeline_events.size()));
                for (const auto &event : clip.timeline_events) {
                    f(event.timestamp);
                    list(event.events);
                }
                i(static_cast<long long>(clip.sound_effects.size()));
                for (const auto &sound : clip.sound_effects) {
                    s(sound.effect_name);
                    f(sound.timestamp);
                }
            }
        }
    }

    void controllers(const std::vector<ysm::binary::ControllerFile> &files) {
        i(static_cast<long long>(files.size()));
        for (const auto &file : files) {
            s(file.name);
            s(file.hash);
            i(file.legacy_unknown_int);
            i(static_cast<long long>(file.controllers.size()));
            for (const auto &entry : file.controllers.entries()) {
                s(entry.first);
                const auto &controller = entry.second;
                s(controller.animation_name);
                s(controller.initial_state);
                i(static_cast<long long>(controller.states.size()));
                for (const auto &state : controller.states) {
                    s(state.name);
                    map(state.animations);
                    map(state.transitions);
                    list(state.on_entry);
                    list(state.on_exit);
                    f(state.blend_transition_value);
                    b(state.blend_via_shortest_path);
                    i(static_cast<long long>(state.blend_transitions.size()));
                    for (const auto &point : state.blend_transitions) {
                        f(point.first);
                        f(point.second);
                    }
                    list(state.sound_effects);
                }
            }
        }
    }

    std::string stream_;
};

} // namespace omm::test
