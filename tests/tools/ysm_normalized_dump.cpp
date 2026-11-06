// Native side of the M2 normalization differential: prints the line format of
// tools/m2/omp-harness/NormalizedDump.java from the native Asset.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "abi/error.hpp"
#include "ysmparser/parse.hpp"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace {

using namespace omm::ysm;

Limits test_limits() {
    Limits limits;
    const char *configured = std::getenv("OMM_TEST_THREADS");
    const unsigned int count = configured == nullptr ? 0u : static_cast<unsigned int>(std::stoul(configured));
    limits.worker_count = count == 0 ? std::max(1u, std::thread::hardware_concurrency()) : count;
    return limits;
}

std::string text(const std::string &value) {
    std::string out;
    for (char c : value) {
        switch (c) {
        case '\\':
            out += "\\\\";
            break;
        case '\t':
            out += "\\t";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        default:
            out += c;
        }
    }
    return out;
}

std::string hex(float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%x", bits);
    return buffer;
}

template <typename Container>
std::string floats(const Container &values) {
    std::string out;
    bool first = true;
    for (float value : values) {
        if (!first) {
            out += ',';
        }
        first = false;
        out += hex(value);
    }
    return out;
}

std::string expression(const std::optional<std::string> &value) {
    return value ? "e:" + text(*value) : "-";
}

std::string join(const std::vector<std::string> &parts) {
    std::string out;
    for (size_t index = 0; index < parts.size(); ++index) {
        if (index != 0) {
            out += '|';
        }
        out += parts[index];
    }
    return out;
}

std::string strings(const std::vector<std::string> &values) {
    std::vector<std::string> parts;
    for (const std::string &value : values) {
        parts.push_back(text(value));
    }
    return join(parts);
}

std::string expression_texts(const std::vector<std::string> &values) {
    std::vector<std::string> parts;
    for (const std::string &value : values) {
        parts.push_back("e:" + text(value));
    }
    return join(parts);
}

std::string component_values(const ComponentVector &vector) {
    return hex(vector[0].value) + "," + hex(vector[1].value) + "," + hex(vector[2].value);
}

std::string component_expressions(const ComponentVector &vector) {
    return expression(vector[0].expression) + "," + expression(vector[1].expression) + "," +
           expression(vector[2].expression);
}

void channel(const char *type, const std::optional<Channel> &value) {
    if (!value) {
        std::printf("CHANNEL\t%s\tnone\n", type);
        return;
    }
    if (!value->keyframed || value->keyframes.empty()) {
        // Object channels without numeric keys have no static expressions in the reference.
        const std::string expressions = value->keyframed ? "-,-,-" : component_expressions(value->constant);
        std::printf("CHANNEL\t%s\tstatic\t%s\t%s\n", type, component_values(value->constant).c_str(),
                    expressions.c_str());
        return;
    }
    std::printf("CHANNEL\t%s\tkeyframes\t%zu\n", type, value->keyframes.size());
    for (const Keyframe &keyframe : value->keyframes) {
        std::printf("KF\t%s\t%s\t%s\t%s\t%s\t%s\n", hex(keyframe.time).c_str(), text(keyframe.interpolation).c_str(),
                    component_values(keyframe.post).c_str(), component_expressions(keyframe.post).c_str(),
                    component_values(keyframe.pre).c_str(), component_expressions(keyframe.pre).c_str());
    }
}

void dump(const Asset &asset) {
    const Manifest &m = asset.manifest;
    std::printf("MANIFEST\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", m.kind == AvatarKind::current ? "NEW" : "OLD",
                text(m.name).c_str(), text(m.description).c_str(), strings(m.authors).c_str(),
                text(m.main_model).c_str(), text(m.arm_model).c_str(), text(m.default_texture).c_str(),
                m.free ? "true" : "false");
    for (const TextureOption &texture : m.textures) {
        std::printf("TEXTURE\t%s\t%s\t%s\t%s\t%s\n", text(texture.id).c_str(), text(texture.name).c_str(),
                    text(texture.path).c_str(), text(texture.normal_path).c_str(), text(texture.specular_path).c_str());
    }
    std::printf("ANIMATIONS\t%s\n", strings(m.animations).c_str());
    std::printf("CONTROLLERS\t%s\n", strings(m.controllers).c_str());
    std::printf("FUNCTIONS\t%s\n", strings(m.functions).c_str());
    std::printf("SOUNDS\t%s\n", strings(m.sounds).c_str());
    std::printf("LANGUAGES\t%s\n", strings(m.languages).c_str());
    std::printf("PROJECTILES\t%s\n", strings(m.projectiles).c_str());
    std::printf("VEHICLES\t%s\n", strings(m.vehicles).c_str());
    std::printf("IMAGES\t%s\t%s\n", text(m.avatar_image).c_str(), text(m.background_image).c_str());
    for (const std::string &warning : m.warnings) {
        std::printf("WARNING\t%s\n", text(warning).c_str());
    }

    // Main then arm geometry, as the reference dump orders them.
    for (const std::string &path : {m.main_model, m.arm_model}) {
        if (path.empty()) {
            continue;
        }
        for (const Geometry &geometry : asset.geometries) {
            if (geometry.path != path) {
                continue;
            }
            std::printf("GEOMETRY\t%s\t%d\t%d\t%zu\n", text(path).c_str(), geometry.texture_width,
                        geometry.texture_height, geometry.bones.size());
            for (const GeometryBone &bone : geometry.bones) {
                std::printf("BONE\t%s\t%s\t%s\t%s\t%zu\n", text(bone.name).c_str(), text(bone.parent).c_str(),
                            floats(bone.pivot).c_str(), floats(bone.rotation).c_str(), bone.cubes.size());
                for (const GeometryCube &cube : bone.cubes) {
                    std::printf("CUBE\t%zu\n", cube.quads.size());
                    for (const Quad &quad : cube.quads) {
                        std::printf("QUAD\t%s\t%s\t%s\n", floats(quad.positions).c_str(), floats(quad.uvs).c_str(),
                                    floats(quad.normal).c_str());
                    }
                }
            }
        }
    }

    static const char *kLoops[] = {"ONCE", "LOOP", "HOLD_ON_LAST_FRAME"};
    for (const AnimationFile &file : asset.animation_files) {
        std::printf("ANIMFILE\t%s\t%s\n", text(file.path).c_str(), file.first_person ? "true" : "false");
        std::map<std::string, const AnimationClip *> clips;
        for (const AnimationClip &clip : file.clips) {
            clips[clip.name] = &clip;
        }
        for (const auto &[name, clip] : clips) {
            const std::vector<std::string> blend = clip->blend_weight_expression
                                                       ? std::vector<std::string>{*clip->blend_weight_expression}
                                                       : std::vector<std::string>{};
            std::printf("CLIP\t%s\t%s\t%s\t%s\t%s\n", text(name).c_str(),
                        clip->unbounded ? "unbounded" : hex(clip->length).c_str(), kLoops[static_cast<int>(clip->loop)],
                        hex(clip->blend_weight).c_str(), expression_texts(blend).c_str());
            std::map<std::string, const BoneAnimation *> bones;
            for (const BoneAnimation &bone : clip->bones) {
                bones[bone.bone] = &bone;
            }
            for (const auto &[bone_name, bone] : bones) {
                std::printf("BONEANIM\t%s\n", text(bone_name).c_str());
                channel("position", bone->position);
                channel("rotation", bone->rotation);
                channel("scale", bone->scale);
            }
            for (const AnimationEvent &event : clip->events) {
                std::map<std::string, std::string> params(event.params.begin(), event.params.end());
                std::vector<std::string> parts;
                for (const auto &[key, value] : params) {
                    parts.push_back(text(key) + "=" + text(value));
                }
                std::printf("EVENT\t%u\t%s\t%s\t%s\t%s\n", event.ordinal, hex(event.time).c_str(), event.type.c_str(),
                            text(event.text).c_str(), join(parts).c_str());
            }
        }
    }

    for (const ControllerFile &file : asset.controller_files) {
        std::printf("CTRLFILE\t%s\n", text(file.path).c_str());
        for (const Controller &controller : file.controllers) {
            std::printf("CTRL\t%s\t%s\n", text(controller.name).c_str(), text(controller.initial_state).c_str());
            for (const ControllerState &state : controller.states) {
                std::vector<std::string> animations;
                for (const auto &animation : state.animations) {
                    animations.push_back(text(animation.animation) + "=" + expression(animation.weight));
                }
                std::vector<std::string> transitions;
                for (const auto &transition : state.transitions) {
                    transitions.push_back(text(transition.target) + "=" + expression(transition.condition));
                }
                std::string blend;
                if (state.blend_curve) {
                    std::vector<float> times;
                    std::vector<float> incoming;
                    for (const auto &point : state.blend_points) {
                        times.push_back(point.first);
                        incoming.push_back(1.0f - point.second);
                    }
                    blend = "curve:" + floats(times) + ":" + floats(incoming);
                } else {
                    blend = "linear:" + hex(state.blend_transition);
                }
                std::printf("STATE\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", text(state.name).c_str(),
                            join(animations).c_str(), join(transitions).c_str(),
                            expression_texts(state.on_entry).c_str(), expression_texts(state.on_exit).c_str(),
                            hex(state.blend_transition).c_str(), blend.c_str(),
                            state.blend_via_shortest_path ? "true" : "false", strings(state.sound_effects).c_str());
            }
        }
    }
}

std::vector<uint8_t> read_file(const std::filesystem::path &path) {
    std::ifstream stream(path, std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

std::string utf8(const std::filesystem::path &path) {
    const auto value = path.u8string();
    return std::string(reinterpret_cast<const char *>(value.data()), value.size());
}

void sample(const std::filesystem::path &path) {
    const std::string name = utf8(path.filename());
    std::printf("SAMPLE\t%s\n", name.c_str());
    try {
        std::unique_ptr<Asset> asset;
        if (std::filesystem::is_directory(path)) {
            std::vector<std::string> names;
            std::vector<std::vector<uint8_t>> contents;
            for (const auto &entry : std::filesystem::recursive_directory_iterator(path)) {
                if (entry.is_regular_file()) {
                    names.push_back(utf8(std::filesystem::relative(entry.path(), path)));
                    contents.push_back(read_file(entry.path()));
                }
            }
            std::vector<EntryInput> entries;
            for (size_t index = 0; index < names.size(); ++index) {
                entries.push_back(EntryInput{names[index], contents[index]});
            }
            asset = parse_entries(entries, name, test_limits());
        } else {
            const std::vector<uint8_t> bytes = read_file(path);
            asset = parse_container(bytes, name, test_limits());
        }
        dump(*asset);
    } catch (const omm::abi::Failure &failure) {
        std::printf("ERROR\t%.*s\n", static_cast<int>(failure.message().size()), failure.message().data());
    }
}

} // namespace

#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
    SetConsoleOutputCP(CP_UTF8);
    int first = 1;
    if (argc >= 3 && std::wstring(argv[1]) == L"--output") {
        FILE *file = nullptr;
        if (_wfreopen_s(&file, argv[2], L"wb", stdout) != 0) {
            return 1;
        }
        first = 3;
    }
    for (int index = first; index < argc; ++index) {
        sample(argv[index]);
    }
    return 0;
}
#else
int main(int argc, char **argv) {
    int first = 1;
    if (argc >= 3 && std::string(argv[1]) == "--output") {
        if (std::freopen(argv[2], "wb", stdout) == nullptr) {
            return 1;
        }
        first = 3;
    }
    for (int index = first; index < argc; ++index) {
        sample(argv[index]);
    }
    return 0;
}
#endif
