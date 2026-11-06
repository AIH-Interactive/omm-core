// Conservative owned-storage accounting, independent of a platform allocator's metadata.
#pragma once

#include <limits>
#include <type_traits>

#include "ysmparser/asset.hpp"

namespace omm::ysm {

class AssetMemoryCounter {
public:
    uint64_t count(const Asset &asset) {
        bytes(sizeof(Asset));
        add(asset.source.zip_root);
        add(asset.metadata_json);
        add(asset.manifest);
        add(asset.geometries);
        add(asset.animation_files);
        add(asset.controller_files);
        add(asset.resources.entries());
        // Two path indices: nodes, buckets and copied keys. Payload strings are counted below.
        bytes(asset.resources.size() * 256ull);
        for (const Resource &resource : asset.resources.entries()) {
            add(resource.path);
            add(resource.path);
        }
        add(asset.resource_info);
        add(asset.diagnostics);
        add(asset.programs);
        return total_;
    }

private:
    void bytes(uint64_t value) {
        if (value > std::numeric_limits<uint64_t>::max() - total_) {
            fail(DecodeCode::resource_limit, "memory/overflow", 0);
        }
        total_ += value;
    }

    template <typename T>
    void add(const T &) {} // Scalars own no additional storage.

    void add(const std::string &text) {
        bytes(text.capacity() + 1);
    }

    void add(const ProgramRef &program) {
        if (program) {
            bytes(sizeof(Program) + 32); // Shared ownership control block.
            add(program->source);
            add(program->bytecode);
        }
    }

    void add(const NamedProgram &value) {
        add(value.name);
        add(value.program);
    }

    template <typename T>
    void add(const std::optional<T> &value) {
        if (value) {
            add(*value);
        }
    }

    template <typename T>
    void add(const std::vector<T> &values) {
        bytes(values.capacity() * static_cast<uint64_t>(sizeof(T)));
        if constexpr (!std::is_trivially_copyable_v<T>) {
            for (const auto &value : values) {
                add(value);
            }
        }
    }

    template <typename A, typename B>
    void add(const std::pair<A, B> &value) {
        add(value.first);
        add(value.second);
    }

    void add(const ComponentVector &values) {
        for (const Component &value : values) {
            add(value.expression);
            add(value.program);
        }
    }

    void add(const Geometry &value) {
        add(value.path);
        add(value.bones);
    }

    void add(const GeometryCube &value) {
        add(value.quads);
    }

    void add(const GeometryBone &value) {
        add(value.name);
        add(value.parent);
        add(value.cubes);
    }

    void add(const Keyframe &value) {
        add(value.interpolation);
        add(value.pre);
        add(value.post);
    }

    void add(const Channel &value) {
        add(value.constant);
        add(value.keyframes);
    }

    void add(const BoneAnimation &value) {
        add(value.bone);
        add(value.position);
        add(value.rotation);
        add(value.scale);
    }

    void add(const AnimationEvent &value) {
        add(value.type);
        add(value.text);
        add(value.params);
        add(value.program);
    }

    void add(const AnimationClip &value) {
        add(value.name);
        add(value.blend_weight_expression);
        add(value.blend_weight_program);
        add(value.bones);
        add(value.events);
    }

    void add(const AnimationFile &value) {
        add(value.path);
        add(value.clips);
        add(value.diagnostics);
    }

    void add(const ControllerAnimationRef &value) {
        add(value.animation);
        add(value.weight);
        add(value.program);
    }

    void add(const ControllerTransition &value) {
        add(value.target);
        add(value.condition);
        add(value.program);
    }

    void add(const ControllerState &value) {
        add(value.name);
        add(value.animations);
        add(value.transitions);
        add(value.on_entry);
        add(value.on_exit);
        add(value.entry_program);
        add(value.exit_program);
        add(value.blend_points);
        add(value.sound_effects);
    }

    void add(const Controller &value) {
        add(value.name);
        add(value.initial_state);
        add(value.states);
    }

    void add(const ControllerFile &value) {
        add(value.path);
        add(value.controllers);
        add(value.diagnostics);
    }

    void add(const Resource &value) {
        add(value.path);
        add(value.bytes);
    }

    void add(const ResourceInfo &value) {
        add(value.media_type);
    }

    void add(const TextureOption &value) {
        add(value.id);
        add(value.name);
        add(value.path);
        add(value.normal_path);
        add(value.specular_path);
    }

    void add(const Manifest &value) {
        add(value.name);
        add(value.description);
        add(value.authors);
        add(value.main_model);
        add(value.arm_model);
        add(value.animations);
        add(value.controllers);
        add(value.first_person_animations);
        add(value.textures);
        add(value.default_texture);
        add(value.functions);
        add(value.sounds);
        add(value.languages);
        add(value.projectiles);
        add(value.vehicles);
        add(value.avatar_image);
        add(value.background_image);
        add(value.gui_foreground);
        add(value.gui_background);
        add(value.thumbnail_image);
        add(value.warnings);
    }

    uint64_t total_ = 0;
};

} // namespace omm::ysm
