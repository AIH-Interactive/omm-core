// Own, redistributable fixture encoded from refs/YesSteveModel/src/main/proto (CC0-1.0).
#pragma once

#include <bit>
#include <string>
#include <vector>

#include "ysmparser/crypto.hpp"

namespace omm::test::current {

using Bytes = std::vector<uint8_t>;

inline void varint(Bytes &out, uint64_t value) {
    while (value >= 128) {
        out.push_back(static_cast<uint8_t>(value) | 128u);
        value >>= 7;
    }
    out.push_back(static_cast<uint8_t>(value));
}

inline void fixed(Bytes &out, uint64_t value, size_t size) {
    for (size_t index = 0; index < size; ++index) {
        out.push_back(static_cast<uint8_t>(value >> (index * 8)));
    }
}

inline void integer(Bytes &out, uint32_t field, uint64_t value) {
    varint(out, static_cast<uint64_t>(field) << 3);
    varint(out, value);
}

inline void data(Bytes &out, uint32_t field, const Bytes &value) {
    varint(out, (static_cast<uint64_t>(field) << 3) | 2u);
    varint(out, value.size());
    out.insert(out.end(), value.begin(), value.end());
}

inline void text(Bytes &out, uint32_t field, const std::string &value) {
    data(out, field, Bytes(value.begin(), value.end()));
}

inline void number(Bytes &out, uint32_t field, float value) {
    varint(out, (static_cast<uint64_t>(field) << 3) | 5u);
    fixed(out, std::bit_cast<uint32_t>(value), 4);
}

inline Bytes map_entry(const std::string &key, const Bytes &value) {
    Bytes entry;
    text(entry, 1, key);
    data(entry, 2, value);
    return entry;
}

inline Bytes source_program(const std::string &source) {
    Bytes result;
    integer(result, 1, 1);
    text(result, 3, source);
    return result;
}

inline Bytes geometry(bool bad_index = false, bool cyclic = false) {
    Bytes bone;
    text(bone, 1, "root");
    if (cyclic) {
        text(bone, 2, "root");
    }
    Bytes pivot;
    Bytes rotation;
    for (float value : {0.0f, 8.0f, 0.0f}) {
        fixed(pivot, std::bit_cast<uint32_t>(value), 4);
    }
    for (size_t index = 0; index < 3; ++index) {
        fixed(rotation, 0, 4);
    }
    data(bone, 3, pivot);
    data(bone, 4, rotation);
    integer(bone, 6, 1);
    Bytes cube;
    integer(cube, 1, 1);
    Bytes positions;
    for (float value : {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f}) {
        fixed(positions, std::bit_cast<uint32_t>(value), 4);
    }
    data(cube, 2, positions);
    data(cube, 3, Bytes{0, 1, 2, static_cast<uint8_t>(bad_index ? 9 : 3)});
    Bytes uv;
    for (float value : {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f}) {
        fixed(uv, std::bit_cast<uint32_t>(value), 4);
    }
    data(cube, 4, uv);
    data(cube, 5, Bytes{0, 1, 2, 3});
    Bytes normal;
    for (float value : {0.0f, 0.0f, 1.0f}) {
        fixed(normal, std::bit_cast<uint32_t>(value), 4);
    }
    data(cube, 6, normal);
    Bytes cubes;
    data(cubes, 1, cube);
    Bytes result;
    data(result, 1, bone);
    data(result, 3, cubes);
    return result;
}

inline Bytes animation(const std::string &name = "idle") {
    Bytes value;
    number(value, 1, 2.0f);
    Bytes frame;
    number(frame, 1, 10.0f);
    integer(frame, 2, 1);
    data(frame, 3, value);
    Bytes bone;
    text(bone, 1, "root");
    data(bone, 3, frame);
    Bytes clip;
    text(clip, 1, name);
    number(clip, 2, 40.0f);
    integer(clip, 3, 1);
    data(clip, 5, bone);
    Bytes blend;
    data(blend, 2, source_program("q.blend"));
    data(clip, 4, blend);
    Bytes event;
    Bytes bytecode;
    integer(bytecode, 1, 7);
    data(bytecode, 2, Bytes{0xFA, 0x00, 0x12});
    data(event, 1, bytecode);
    number(event, 2, 10.0f);
    data(clip, 6, event);
    Bytes sound;
    text(sound, 1, "step");
    number(sound, 2, 10.0f);
    data(clip, 7, sound);
    Bytes file;
    data(file, 1, clip);
    return file;
}

inline Bytes controller() {
    Bytes state;
    text(state, 1, "default");
    data(state, 4, source_program("v.enter=1;"));
    Bytes reference;
    text(reference, 1, "idle");
    data(reference, 2, source_program("q.active"));
    data(state, 2, reference);
    Bytes controller;
    text(controller, 1, "main");
    data(controller, 3, state);
    Bytes file;
    data(file, 1, controller);
    return file;
}

inline Bytes container(bool bad_index = false, bool cyclic = false, bool conflicting_role = false) {
    Bytes definition;
    data(definition, 1, map_entry("main", geometry(bad_index, cyclic)));
    Bytes animations = animation();
    const Bytes second_clip = animation("idle2");
    animations.insert(animations.end(), second_clip.begin(), second_clip.end());
    data(definition, 2, map_entry("fp_arm", animations));
    data(definition, 2, map_entry("main", animations));
    data(definition, 3, map_entry("main", controller()));
    data(definition, 3, map_entry("aux", controller()));
    Bytes image;
    integer(image, 1, conflicting_role ? 1 : 2);
    text(image, 2, "png");
    integer(image, 3, 1);
    integer(image, 4, 1);
    Bytes texture;
    data(texture, 1, image);
    Bytes target;
    text(target, 1, "player");
    integer(target, 2, 1);
    integer(target, 4, 1);
    data(target, 5, map_entry("default", texture));
    Bytes properties;
    data(properties, 1, Bytes(32));
    integer(properties, 2, 1);
    Bytes metadata;
    text(metadata, 1, "Current fixture");
    Bytes info;
    data(info, 2, properties);
    data(info, 4, metadata);
    Bytes manifest;
    data(manifest, 1, target);
    data(manifest, 3, info);

    Bytes tlv;
    auto short_text = [](Bytes &out, const std::string &value) {
        out.push_back(static_cast<uint8_t>(value.size()));
        out.insert(out.end(), value.begin(), value.end());
    };
    fixed(tlv, 0, 2);
    short_text(tlv, "0.1.1-unstable");
    fixed(tlv, 2, 2);
    short_text(tlv, std::string(64, '0'));
    const std::vector<std::pair<std::string, Bytes>> chunks{
        {"manifest", manifest}, {"blob-1", definition}, {"blob-2", Bytes{0x89, 'P', 'N', 'G'}}};
    Bytes table;
    auto descriptor = [&](const std::string &name, const std::string &encoding, const Bytes &bytes, bool verify) {
        short_text(table, name);
        short_text(table, encoding);
        fixed(table, bytes.size(), 4);
        fixed(table, 0, 4);
        fixed(table, 0, 4);
        table.push_back(0);
        const auto hash = verify ? std::array<uint8_t, 32>{} : ysm::crypto::blake3(bytes);
        table.insert(table.end(), hash.begin(), hash.end());
    };
    descriptor("verification", "BLAKE3", Bytes(34), true);
    for (const auto &[name, bytes] : chunks) {
        descriptor(name, "", bytes, false);
    }
    Bytes result{0xEF, 0xBB, 0xBF, 'Y', 'S', 'G', 'P', '2', '\n', '\n', 0};
    fixed(result, 64, 2);
    fixed(result, 0, 1);
    fixed(result, 1, 2);
    fixed(result, 0, 2);
    auto fixed_text = [](Bytes &out, const std::string &value, size_t length) {
        out.insert(out.end(), value.begin(), value.end());
        out.resize(out.size() + length - value.size());
    };
    fixed_text(result, "unstable", 13);
    fixed_text(result, "mixel/character", 32);
    fixed(result, 2, 2);
    fixed(result, chunks.size() + 1, 2);
    fixed(result, tlv.size(), 4);
    fixed(result, table.size(), 4);
    result.insert(result.end(), tlv.begin(), tlv.end());
    result.insert(result.end(), table.begin(), table.end());
    const auto hash = ysm::crypto::blake3(result);
    result.insert(result.end(), hash.begin(), hash.end());
    fixed(result, 0, 2);
    for (const auto &[name, bytes] : chunks) {
        result.insert(result.end(), bytes.begin(), bytes.end());
    }
    return result;
}

} // namespace omm::test::current
