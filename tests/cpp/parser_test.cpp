// ysmparser C ABI tests on synthetic fixtures (no third-party model data).
//
// Covers: animation fields and event order, controller semantics, ZIP root discovery
// (recursive, shallowest wins, ambiguity rejected), corruption / limit / cleanup paths,
// section capacity protocol, chunked resource reads and handle lifetime.
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "omm/omm.h"
#include "yyjson.h"

namespace {

int failures = 0;

#define EXPECT(condition)                                                                                              \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);                                  \
            ++failures;                                                                                                \
        }                                                                                                              \
    } while (0)

omm_error new_error() {
    omm_error error{};
    error.struct_size = sizeof(error);
    return error;
}

omm_bytes bytes_of(const std::string &text) {
    return omm_bytes{reinterpret_cast<const uint8_t *>(text.data()), text.size()};
}

struct File {
    std::string path;
    std::string data;
};

// ---- minimal stored (method 0) ZIP writer for fixtures ----
uint32_t crc32(const std::string &data) {
    uint32_t crc = 0xFFFFFFFFu;
    for (unsigned char c : data) {
        crc ^= c;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

void put16(std::string &out, uint32_t value) {
    out += static_cast<char>(value & 0xFF);
    out += static_cast<char>((value >> 8) & 0xFF);
}

void put32(std::string &out, uint32_t value) {
    put16(out, value & 0xFFFF);
    put16(out, value >> 16);
}

std::string make_zip(const std::vector<File> &files) {
    std::string archive;
    std::string central;
    for (const File &file : files) {
        const uint32_t offset = static_cast<uint32_t>(archive.size());
        const uint32_t crc = crc32(file.data);
        archive += "PK\x03\x04";
        put16(archive, 20);
        put16(archive, 0x0800); // UTF-8 names
        put16(archive, 0);      // stored
        put16(archive, 0);
        put16(archive, 0);
        put32(archive, crc);
        put32(archive, static_cast<uint32_t>(file.data.size()));
        put32(archive, static_cast<uint32_t>(file.data.size()));
        put16(archive, static_cast<uint32_t>(file.path.size()));
        put16(archive, 0);
        archive += file.path;
        archive += file.data;

        central += "PK\x01\x02";
        put16(central, 20);
        put16(central, 20);
        put16(central, 0x0800);
        put16(central, 0);
        put16(central, 0);
        put16(central, 0);
        put32(central, crc);
        put32(central, static_cast<uint32_t>(file.data.size()));
        put32(central, static_cast<uint32_t>(file.data.size()));
        put16(central, static_cast<uint32_t>(file.path.size()));
        put16(central, 0);
        put16(central, 0);
        put16(central, 0);
        put16(central, 0);
        put32(central, 0);
        put32(central, offset);
        central += file.path;
    }
    const uint32_t central_offset = static_cast<uint32_t>(archive.size());
    archive += central;
    archive += "PK\x05\x06";
    put16(archive, 0);
    put16(archive, 0);
    put16(archive, static_cast<uint32_t>(files.size()));
    put16(archive, static_cast<uint32_t>(files.size()));
    put32(archive, static_cast<uint32_t>(central.size()));
    put32(archive, central_offset);
    put16(archive, 0);
    return archive;
}

// ---- fixture model ----
const char *kManifest = R"({
  "spec": 2,
  "metadata": {"name": "Fixture", "tips": "synthetic", "authors": [{"name": "omm"}]},
  "properties": {"default_texture": "skin", "free": true},
  "files": {"player": {
    "model": {"main": "models/main.json"},
    "texture": [{"path": "textures/skin.png", "id": "skin"}],
    "animation": {"main": "animations/main.animation.json"},
    "animation_controllers": ["controller/main.json"]
  }}
})";

const char *kGeometry = R"({"minecraft:geometry": [{
  "description": {"texture_width": 32, "texture_height": 32},
  "bones": [
    {"name": "root", "pivot": [0, 0, 0]},
    {"name": "body", "parent": "root", "pivot": [0, 8, 0],
     "cubes": [{"origin": [-4, 0, -2], "size": [8, 8, 4], "uv": [0, 0]}]}
  ]}]})";

const char *kAnimations = R"({"animations": {
  "derived": {
    "bones": {"body": {"rotation": {"0.5": [10, 0, 0], "0.25": {"pre": [1, 2, 3], "post": [4, "q.y", 6], "lerp_mode": "catmullrom"}}}},
    "timeline": {"0.5": ["a", "b"], "0.1": "c"},
    "sound_effects": {"0.5": {"effect": "s"}}
  },
  "empty": {},
  "looping": {"loop": true, "animation_length": 2},
  "holding": {"loop": "hold_on_last_frame", "animation_length": 1, "blend_weight": "query.x",
              "bones": {"body": {"position": ["1", "q.y", 3], "scale": 2}}}
}})";

const char *kController = R"({"animation_controllers": {
  "controller.main": {
    "initial_state": "missing",
    "states": {
      "idle": {"animations": ["looping", {"holding": "q.blend"}], "transitions": [{"walk": "q.moving"}],
               "blend_transition": {"0.0": 1.0, "0.2": 0.0}, "on_entry": ["v.a = 1;", ""]},
      "walk": {"animations": {"derived": ""}, "blend_transition": 0.3, "blend_via_shortest_path": true}
    }
  }
}})";

std::vector<File> fixture_files(const std::string &prefix = "") {
    return {
        {prefix + "ysm.json", kManifest},
        {prefix + "models/main.json", kGeometry},
        {prefix + "animations/main.animation.json", kAnimations},
        {prefix + "controller/main.json", kController},
        {prefix + "textures/skin.png", std::string("\x89PNG\r\n\x1a\n", 8) + std::string(64, 'x')},
    };
}

omm_handle create_runtime(uint32_t worker_count = 0, uint64_t memory_budget = 0) {
    omm_error error = new_error();
    omm_runtime_desc desc{};
    desc.struct_size = sizeof(desc);
    desc.version = OMM_RUNTIME_DESC_VERSION;
    desc.requested_abi_major = OMM_ABI_MAJOR;
    desc.requested_abi_minor = OMM_ABI_MINOR;
    desc.worker_count = worker_count;
    desc.cpu_memory_budget = memory_budget;
    omm_handle runtime = 0;
    EXPECT(omm_runtime_create(&desc, &runtime, &error) == omm_ok);
    return runtime;
}

omm_ysm_parse_desc default_desc() {
    omm_ysm_parse_desc desc{};
    desc.struct_size = sizeof(desc);
    desc.version = OMM_YSM_PARSE_DESC_VERSION;
    return desc;
}

omm_status parse_files(omm_handle runtime, const std::vector<File> &files, omm_handle *asset,
                       const omm_ysm_parse_desc *desc = nullptr, omm_error *error = nullptr) {
    std::vector<omm_ysm_entry> entries;
    for (const File &file : files) {
        entries.push_back(omm_ysm_entry{bytes_of(file.path), bytes_of(file.data)});
    }
    omm_error local = new_error();
    return omm_ysm_parse_entries(runtime, entries.data(), static_cast<uint32_t>(entries.size()), desc, asset,
                                 error != nullptr ? error : &local);
}

std::string section(omm_handle asset, uint32_t id) {
    omm_error error = new_error();
    uint64_t required = 0;
    if (omm_asset_read_section(asset, id, omm_mut_bytes{nullptr, 0}, &required, &error) != omm_ok) {
        return std::string();
    }
    std::string text(static_cast<size_t>(required), '\0');
    omm_asset_read_section(asset, id, omm_mut_bytes{reinterpret_cast<uint8_t *>(text.data()), required}, &required,
                           &error);
    return text;
}

void test_thread_configuration() {
    omm_error error = new_error();
    std::vector<std::string> expected;
    for (uint32_t workers : {1u, 2u, 0u, 65u}) {
        const omm_handle runtime = create_runtime(workers);
        omm_runtime_caps caps{};
        caps.struct_size = sizeof(caps);
        EXPECT(omm_runtime_get_caps(runtime, &caps, &error) == omm_ok);
        const uint32_t hardware = std::thread::hardware_concurrency();
        EXPECT(caps.worker_count == (workers == 0 ? (hardware == 0 ? 1 : hardware) : workers));
        omm_handle asset = 0;
        EXPECT(parse_files(runtime, fixture_files(), &asset) == omm_ok);
        for (uint32_t id = omm_section_metadata; id <= omm_section_diagnostics; ++id) {
            const std::string actual = section(asset, id);
            if (workers == 1) {
                expected.push_back(actual);
            } else {
                EXPECT(actual == expected[id - omm_section_metadata]);
            }
        }
        EXPECT(omm_asset_release(asset, &error) == omm_ok);

        // A shallow limit rejects required metadata/geometry.
        omm_ysm_parse_desc desc = default_desc();
        desc.nesting_limit = 4;
        EXPECT(parse_files(runtime, fixture_files(), &asset, &desc) == omm_error_format);
        EXPECT(asset == 0);
        // Motion JSON is deeper than geometry; its rejection becomes a diagnostic,
        // including when it is parsed on another execution thread.
        desc.nesting_limit = 8;
        auto deep_files = fixture_files();
        deep_files[2].data = "{\"unused\":" + std::string(12, '[') + "0" + std::string(12, ']') + "}";
        deep_files[3].data = deep_files[2].data;
        EXPECT(parse_files(runtime, deep_files, &asset, &desc) == omm_ok);
        EXPECT(section(asset, omm_section_diagnostics).find("nesting deeper than 8") != std::string::npos);
        EXPECT(omm_asset_release(asset, &error) == omm_ok);
        EXPECT(omm_runtime_destroy(runtime, &error) == omm_ok);
    }
}

void test_runtime_memory_budget() {
    omm_error error = new_error();
    const auto files = fixture_files();
    const omm_handle reference = create_runtime(1);
    omm_handle asset = 0;
    EXPECT(parse_files(reference, files, &asset) == omm_ok);
    omm_asset_info info{};
    info.struct_size = sizeof(info);
    EXPECT(omm_asset_get_info(asset, &info, &error) == omm_ok);
    const uint64_t model_bytes = info.retained_bytes;
    EXPECT(model_bytes > 0);
    EXPECT(omm_asset_release(asset, &error) == omm_ok);
    EXPECT(omm_runtime_destroy(reference, &error) == omm_ok);

    const omm_handle limited = create_runtime(1, model_bytes + 1);
    EXPECT(parse_files(limited, files, &asset) == omm_ok);
    omm_handle second = 123;
    EXPECT(parse_files(limited, files, &second, nullptr, &error) == omm_error_limit);
    EXPECT(second == 0);
    uint64_t required = 0;
    EXPECT(omm_asset_read_section(asset, omm_section_animations, omm_mut_bytes{}, &required, &error) ==
           omm_error_limit);
    EXPECT(omm_asset_release(asset, &error) == omm_ok);
    // Rejected publications and cached sections must not leak reservations or children.
    EXPECT(parse_files(limited, files, &asset) == omm_ok);
    EXPECT(omm_asset_release(asset, &error) == omm_ok);
    EXPECT(omm_runtime_destroy(limited, &error) == omm_ok);

    const omm_handle tiny = create_runtime(1, 64);
    EXPECT(parse_files(tiny, files, &asset) == omm_error_limit);
    EXPECT(asset == 0);
    EXPECT(omm_runtime_destroy(tiny, &error) == omm_ok);
}

void test_parallel_clip_error_order() {
    auto files = fixture_files();
    files[2].data = R"({"animations": {
        "before": {"animation_length": 1},
        "bad": {"animation_length": {}},
        "after": {"animation_length": 2}
    }})";
    std::vector<std::string> expected;
    omm_error error = new_error();
    for (uint32_t workers : {1u, 4u}) {
        const omm_handle runtime = create_runtime(workers);
        omm_handle asset = 0;
        EXPECT(parse_files(runtime, files, &asset) == omm_ok);
        const std::string animations = section(asset, omm_section_animations);
        const std::string diagnostics = section(asset, omm_section_diagnostics);
        EXPECT(animations.find("before") != std::string::npos);
        EXPECT(animations.find("after") == std::string::npos);
        EXPECT(diagnostics.find("animation_length") != std::string::npos);
        if (workers == 1) {
            expected = {animations, diagnostics};
        } else {
            EXPECT(animations == expected[0] && diagnostics == expected[1]);
        }
        EXPECT(omm_asset_release(asset, &error) == omm_ok);
        EXPECT(omm_runtime_destroy(runtime, &error) == omm_ok);
    }
}

// JSON lookup helpers for assertions.
yyjson_val *find_named(yyjson_val *array, const char *name) {
    size_t index;
    size_t max;
    yyjson_val *item;
    yyjson_arr_foreach(array, index, max, item) {
        yyjson_val *value = yyjson_obj_get(item, "name");
        if (value != nullptr && std::strcmp(yyjson_get_str(value), name) == 0) {
            return item;
        }
    }
    return nullptr;
}

std::string str(yyjson_val *value) {
    return value != nullptr && yyjson_is_str(value) ? yyjson_get_str(value) : std::string("<none>");
}

double num(yyjson_val *value) {
    return value != nullptr && yyjson_is_num(value) ? yyjson_get_num(value) : -999.0;
}

void test_geometry_parent_cycles(omm_handle runtime) {
    for (const std::string &bones :
         {std::string(R"([{"name":"root","parent":"root"}])"),
          std::string(R"([{"name":"root","parent":"body"},{"name":"body","parent":"root"}])")}) {
        auto files = fixture_files();
        files[1].data = "{\"minecraft:geometry\":[{\"bones\":" + bones + "}]}";
        omm_handle asset = 0;
        omm_error error = new_error();
        EXPECT(parse_files(runtime, files, &asset, nullptr, &error) == omm_error_format);
        EXPECT(asset == 0);
        EXPECT(std::strstr(error.message, "parent-cycle") != nullptr);
    }
}

void test_animation_fields(omm_handle runtime) {
    omm_handle asset = 0;
    EXPECT(parse_files(runtime, fixture_files(), &asset) == omm_ok);
    omm_error error = new_error();
    omm_asset_info info{};
    info.struct_size = sizeof(info);
    EXPECT(omm_asset_get_info(asset, &info, &error) == omm_ok);
    EXPECT(info.source_format == omm_ysm_raw);
    EXPECT(info.geometry_count == 1 && info.bone_count == 2 && info.quad_count == 6);
    EXPECT(info.animation_file_count == 1 && info.clip_count == 4 && info.controller_count == 1);

    const std::string json = section(asset, omm_section_animations);
    yyjson_doc *doc = yyjson_read(json.data(), json.size(), 0);
    EXPECT(doc != nullptr);
    yyjson_val *clips = yyjson_obj_get(yyjson_arr_get(yyjson_obj_get(yyjson_doc_get_root(doc), "records"), 0), "clips");

    yyjson_val *derived = find_named(clips, "derived");
    EXPECT(num(yyjson_obj_get(derived, "length")) == 0.5); // derived from the last keyframe
    EXPECT(str(yyjson_obj_get(derived, "loop")) == "once");
    yyjson_val *rotation = yyjson_obj_get(yyjson_arr_get(yyjson_obj_get(derived, "bones"), 0), "rotation");
    yyjson_val *keyframes = yyjson_obj_get(rotation, "keyframes");
    EXPECT(yyjson_arr_size(keyframes) == 2);
    yyjson_val *first = yyjson_arr_get(keyframes, 0);
    EXPECT(num(yyjson_obj_get(first, "time")) == 0.25); // sorted by time
    EXPECT(str(yyjson_obj_get(first, "interpolation")) == "catmullrom");
    EXPECT(num(yyjson_obj_get(yyjson_arr_get(yyjson_obj_get(first, "pre"), 2), "value")) == 3.0);
    EXPECT(str(yyjson_obj_get(yyjson_arr_get(yyjson_obj_get(first, "post"), 1), "expression")) == "q.y");

    // Same-time events keep timeline order, then sound order (stable sort), with ordinals.
    yyjson_val *events = yyjson_obj_get(derived, "events");
    EXPECT(yyjson_arr_size(events) == 4);
    const char *expected[] = {"c", "a", "b", "s"};
    for (size_t index = 0; index < 4; ++index) {
        yyjson_val *event = yyjson_arr_get(events, index);
        EXPECT(str(yyjson_obj_get(event, "text")) == expected[index]);
        EXPECT(num(yyjson_obj_get(event, "ordinal")) == static_cast<double>(index));
    }
    EXPECT(str(yyjson_obj_get(yyjson_arr_get(events, 3), "type")) == "sound");

    yyjson_val *empty = find_named(clips, "empty");
    EXPECT(yyjson_is_null(yyjson_obj_get(empty, "length"))); // unbounded, never Infinity in JSON
    EXPECT(yyjson_get_bool(yyjson_obj_get(empty, "unbounded")));

    EXPECT(str(yyjson_obj_get(find_named(clips, "looping"), "loop")) == "loop");
    yyjson_val *holding = find_named(clips, "holding");
    EXPECT(str(yyjson_obj_get(holding, "loop")) == "hold_on_last_frame");
    yyjson_val *blend = yyjson_obj_get(holding, "blend_weight");
    EXPECT(num(yyjson_obj_get(blend, "value")) == 0.0 && str(yyjson_obj_get(blend, "expression")) == "query.x");
    yyjson_val *holding_bone = yyjson_arr_get(yyjson_obj_get(holding, "bones"), 0);
    yyjson_val *position = yyjson_obj_get(yyjson_obj_get(holding_bone, "position"), "constant");
    EXPECT(num(yyjson_obj_get(yyjson_arr_get(position, 0), "value")) == 1.0); // numeric string is a constant
    EXPECT(str(yyjson_obj_get(yyjson_arr_get(position, 1), "expression")) == "q.y");
    yyjson_val *scale = yyjson_obj_get(yyjson_obj_get(holding_bone, "scale"), "constant");
    EXPECT(num(yyjson_obj_get(yyjson_arr_get(scale, 2), "value")) == 2.0);
    yyjson_doc_free(doc);

    // Controllers: unknown initial state falls back to the first state; curves keep points.
    const std::string controllers = section(asset, omm_section_controllers);
    yyjson_doc *cdoc = yyjson_read(controllers.data(), controllers.size(), 0);
    yyjson_val *controller = yyjson_arr_get(
        yyjson_obj_get(yyjson_arr_get(yyjson_obj_get(yyjson_doc_get_root(cdoc), "records"), 0), "controllers"), 0);
    EXPECT(str(yyjson_obj_get(controller, "initial_state")) == "idle");
    yyjson_val *idle = find_named(yyjson_obj_get(controller, "states"), "idle");
    EXPECT(yyjson_arr_size(yyjson_obj_get(idle, "blend_curve")) == 2);
    EXPECT(yyjson_arr_size(yyjson_obj_get(idle, "on_entry")) == 1); // blank entries dropped
    yyjson_val *animations = yyjson_obj_get(idle, "animations");
    EXPECT(yyjson_is_null(yyjson_obj_get(yyjson_arr_get(animations, 0), "weight")));
    EXPECT(str(yyjson_obj_get(yyjson_arr_get(animations, 1), "weight")) == "q.blend");
    yyjson_val *walk = find_named(yyjson_obj_get(controller, "states"), "walk");
    EXPECT(yyjson_is_null(yyjson_obj_get(walk, "blend_curve")));
    EXPECT(yyjson_get_bool(yyjson_obj_get(walk, "blend_via_shortest_path")));
    yyjson_doc_free(cdoc);

    // Metadata: resource references resolve to ids, missing ones are null.
    const std::string metadata = section(asset, omm_section_metadata);
    EXPECT(metadata.find("\"name\":\"Fixture\"") != std::string::npos);
    EXPECT(metadata.find("\"default_texture\":\"skin\"") != std::string::npos);
    EXPECT(omm_asset_release(asset, &error) == omm_ok);
}

void test_zip_roots(omm_handle runtime) {
    omm_error error = new_error();
    const omm_ysm_parse_desc desc = default_desc();

    // Nested root: the model lives two directories deep next to unrelated files.
    std::vector<File> nested = fixture_files("pack/inner/");
    nested.push_back({"pack/readme.txt", "not part of the model"});
    const std::string zip = make_zip(nested);
    omm_ysm_probe_result probe{};
    probe.struct_size = sizeof(probe);
    EXPECT(omm_ysm_probe(bytes_of(zip), &probe, &error) == omm_ok && probe.format == omm_ysm_zip);
    omm_handle asset = 0;
    EXPECT(omm_ysm_parse(runtime, bytes_of(zip), &desc, &asset, &error) == omm_ok);
    EXPECT(section(asset, omm_section_metadata).find("\"zip_root\":\"pack/inner\"") != std::string::npos);
    const std::string resources = section(asset, omm_section_resources);
    EXPECT(resources.find("readme.txt") == std::string::npos);            // outside the root
    EXPECT(resources.find("\"path\":\"ysm.json\"") != std::string::npos); // root-relative
    omm_asset_release(asset, &error);

    // Shallowest metadata wins.
    std::vector<File> layered = fixture_files("a/");
    for (const File &file : fixture_files("a/b/")) {
        layered.push_back(file);
    }
    const std::string layered_zip = make_zip(layered);
    EXPECT(omm_ysm_parse(runtime, bytes_of(layered_zip), &desc, &asset, &error) == omm_ok);
    EXPECT(section(asset, omm_section_metadata).find("\"zip_root\":\"a\"") != std::string::npos);
    omm_asset_release(asset, &error);

    // Two roots at the same depth are ambiguous.
    std::vector<File> twins = fixture_files("left/");
    for (const File &file : fixture_files("right/")) {
        twins.push_back(file);
    }
    const std::string twins_zip = make_zip(twins);
    asset = 123;
    EXPECT(omm_ysm_parse(runtime, bytes_of(twins_zip), &desc, &asset, &error) == omm_error_format);
    EXPECT(asset == 0);
    EXPECT(std::strstr(error.message, "ambiguous-root") != nullptr);

    // No metadata file at all.
    const std::string bare = make_zip({{"x/readme.txt", "hello"}});
    EXPECT(omm_ysm_parse(runtime, bytes_of(bare), &desc, &asset, &error) == omm_error_format);
    EXPECT(std::strstr(error.message, "metadata-not-found") != nullptr);
}

void test_errors_and_limits(omm_handle runtime) {
    omm_error error = new_error();
    omm_handle asset = 77;
    const omm_ysm_parse_desc desc = default_desc();

    EXPECT(omm_ysm_parse(runtime, bytes_of("not a model"), &desc, &asset, &error) == omm_error_unsupported);
    EXPECT(asset == 0);

    // Text-header YSGP v3 that is far too short: TRUNCATED, reported as a format error.
    std::string v3 = "\xEF\xBB\xBFYSGP\r\nheader";
    v3 += '\0';
    v3 += std::string("\x03\x00\x00\x00", 4);
    omm_ysm_probe_result probe{};
    probe.struct_size = sizeof(probe);
    EXPECT(omm_ysm_probe(bytes_of(v3), &probe, &error) == omm_ok);
    EXPECT(probe.format == omm_ysm_legacy_v3 && probe.source_version == 3 && probe.parseable == 1);
    EXPECT(omm_ysm_parse(runtime, bytes_of(v3), &desc, &asset, &error) == omm_error_format);
    EXPECT(std::strstr(error.message, "TRUNCATED") != nullptr);

    // v3 of plausible size with a wrong integrity hash.
    std::string corrupt = v3 + std::string(256, '\x5A');
    EXPECT(omm_ysm_parse(runtime, bytes_of(corrupt), &desc, &asset, &error) == omm_error_format);
    EXPECT(std::strstr(error.message, "CHECKSUM_FAILED") != nullptr);

    // Legacy v2 container whose MD5 does not match.
    std::string legacy = std::string("YSGP\x00\x00\x00\x02", 8) + std::string(16, '\0') + std::string(40, '\x11');
    EXPECT(omm_ysm_parse(runtime, bytes_of(legacy), &desc, &asset, &error) == omm_error_format);
    EXPECT(std::strstr(error.message, "CHECKSUM_FAILED") != nullptr);

    // Unknown YSGP crypto version.
    std::string v9 = "YSGPmeta";
    v9 += '\0';
    v9 += std::string("\x09\x00\x00\x00", 4) + std::string(80, '\0');
    EXPECT(omm_ysm_parse(runtime, bytes_of(v9), &desc, &asset, &error) == omm_error_unsupported);

    // Raw entries: path traversal and case-only duplicates.
    std::vector<File> traversal = fixture_files();
    traversal.push_back({"../escape.json", "{}"});
    EXPECT(parse_files(runtime, traversal, &asset, &desc, &error) == omm_error_format);
    EXPECT(std::strstr(error.message, "INVALID_PATH") != nullptr);
    std::vector<File> conflict = fixture_files();
    conflict.push_back({"YSM.JSON", "{}"});
    EXPECT(parse_files(runtime, conflict, &asset, &desc, &error) == omm_error_format);
    EXPECT(std::strstr(error.message, "PATH_CONFLICT") != nullptr);

    // Limits.
    omm_ysm_parse_desc limited = default_desc();
    limited.expanded_bytes_limit = 64;
    EXPECT(parse_files(runtime, fixture_files(), &asset, &limited, &error) == omm_error_limit);
    limited = default_desc();
    limited.entry_count_limit = 2;
    EXPECT(parse_files(runtime, fixture_files(), &asset, &limited, &error) == omm_error_limit);
    limited = default_desc();
    limited.bone_limit = 1;
    EXPECT(parse_files(runtime, fixture_files(), &asset, &limited, &error) == omm_error_limit);
    limited = default_desc();
    limited.keyframe_limit = 1;
    EXPECT(parse_files(runtime, fixture_files(), &asset, &limited, &error) == omm_error_limit);
    EXPECT(asset == 0);

    // Nesting limit protects the recursive JSON walkers.
    std::vector<File> deep = fixture_files();
    deep[0].data = std::string(2000, '[') + std::string(2000, ']');
    EXPECT(parse_files(runtime, deep, &asset, &desc, &error) == omm_error_format);
    EXPECT(std::strstr(error.message, "nesting") != nullptr);

    // Not a YSM model (Figura avatar layout).
    EXPECT(parse_files(runtime, {{"avatar.json", "{}"}}, &asset, &desc, &error) == omm_error_format);
    EXPECT(std::strstr(error.message, "UNRECOGNIZED_SOURCE") != nullptr);

    // Profile and descriptor validation.
    omm_ysm_parse_desc profile = default_desc();
    profile.source_profile = omm_profile_ysm_current;
    EXPECT(parse_files(runtime, fixture_files(), &asset, &profile, &error) == omm_error_unsupported);
    omm_ysm_parse_desc small = default_desc();
    small.struct_size = 8;
    EXPECT(parse_files(runtime, fixture_files(), &asset, &small, &error) == omm_error_argument);
    omm_ysm_parse_desc formats = default_desc();
    formats.allowed_formats = 1u << omm_ysm_zip;
    EXPECT(parse_files(runtime, fixture_files(), &asset, &formats, &error) == omm_error_unsupported);
}

void test_reads_and_lifetime() {
    omm_error error = new_error();
    const omm_handle runtime = create_runtime();
    omm_handle asset = 0;
    EXPECT(parse_files(runtime, fixture_files(), &asset) == omm_ok);

    // Destroying the runtime with a live asset is a state error and keeps both alive.
    EXPECT(omm_runtime_destroy(runtime, &error) == omm_error_state);

    // Capacity protocol: too small → capacity, nothing written.
    uint64_t required = 0;
    EXPECT(omm_asset_read_section(asset, omm_section_resources, omm_mut_bytes{nullptr, 0}, &required, &error) ==
           omm_ok);
    EXPECT(required > 16);
    std::vector<uint8_t> small(16, 0xCD);
    EXPECT(omm_asset_read_section(asset, omm_section_resources, omm_mut_bytes{small.data(), small.size()}, &required,
                                  &error) == omm_error_capacity);
    EXPECT(small[0] == 0xCD && small[15] == 0xCD);
    EXPECT(omm_asset_read_section(asset, 99, omm_mut_bytes{nullptr, 0}, &required, &error) == omm_error_argument);

    // Chunked resource read of the texture (resource ids follow entry order: index 4).
    std::string collected;
    uint8_t chunk[7];
    uint64_t offset = 0;
    for (;;) {
        uint64_t written = 0;
        EXPECT(omm_asset_read_resource(asset, 4, offset, omm_mut_bytes{chunk, sizeof(chunk)}, &written, &error) ==
               omm_ok);
        if (written == 0) {
            break;
        }
        collected.append(reinterpret_cast<const char *>(chunk), static_cast<size_t>(written));
        offset += written;
    }
    EXPECT(collected == fixture_files()[4].data);
    uint64_t written = 1;
    EXPECT(omm_asset_read_resource(asset, 999, 0, omm_mut_bytes{chunk, sizeof(chunk)}, &written, &error) ==
           omm_error_argument);

    EXPECT(omm_asset_release(asset, &error) == omm_ok);
    EXPECT(omm_asset_release(asset, &error) == omm_error_handle);
    omm_asset_info info{};
    info.struct_size = sizeof(info);
    EXPECT(omm_asset_get_info(asset, &info, &error) == omm_error_handle);
    EXPECT(omm_asset_get_info(runtime, &info, &error) == omm_error_handle); // wrong handle kind
    EXPECT(omm_runtime_destroy(runtime, &error) == omm_ok);

    // Parsing against a destroyed runtime fails cleanly.
    EXPECT(parse_files(runtime, fixture_files(), &asset) == omm_error_handle);
    EXPECT(asset == 0);
}

} // namespace

int main() {
    test_thread_configuration();
    test_runtime_memory_budget();
    test_parallel_clip_error_order();
    const omm_handle runtime = create_runtime();
    omm_runtime_caps caps{};
    caps.struct_size = sizeof(caps);
    omm_error error = new_error();
    EXPECT(omm_runtime_get_caps(runtime, &caps, &error) == omm_ok);
    EXPECT((caps.parser_formats & (1u << omm_ysm_legacy_v3)) != 0);
    EXPECT((caps.parser_formats & (1u << omm_ysm_asset_container)) != 0);

    test_animation_fields(runtime);
    test_geometry_parent_cycles(runtime);
    test_zip_roots(runtime);
    test_errors_and_limits(runtime);
    EXPECT(omm_runtime_destroy(runtime, &error) == omm_ok);
    test_reads_and_lifetime();

    if (failures != 0) {
        std::fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    std::printf("parser_test: all checks passed\n");
    return 0;
}
