#include <cstdio>
#include <fstream>
#include <string>

#include "../support/current_fixture.hpp"
#include "omm/omm.h"
#include "ysmparser/parse.hpp"
#include "yyjson.h"

namespace {

int failures = 0;

void expect(bool value, const char *message) {
    if (!value) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

omm_error error_value() {
    omm_error error{};
    error.struct_size = sizeof(error);
    return error;
}

std::string section(omm_handle asset, uint32_t id) {
    auto error = error_value();
    uint64_t required = 0;
    expect(omm_asset_read_section(asset, id, {}, &required, &error) == omm_ok, "current section sizing");
    std::string result(static_cast<size_t>(required), '\0');
    expect(omm_asset_read_section(asset, id, {reinterpret_cast<uint8_t *>(result.data()), required}, &required,
                                  &error) == omm_ok,
           "current section copy");
    yyjson_doc *document = yyjson_read(result.data(), result.size(), 0);
    expect(document != nullptr, "current section is valid JSON");
    yyjson_doc_free(document);
    return result;
}

void test_current_asset() {
    const auto fixture = omm::test::current::container();
    auto asset = omm::ysm::parse_container(fixture, "fallback", {});
    expect(asset->source.container == omm::ysm::SourceContainer::current, "current source identity");
    expect(asset->manifest.name == "Current fixture", "current metadata");
    expect(asset->geometries.size() == 1, "current geometry count");
    const auto &geometry = asset->geometries.front();
    expect(geometry.bones.size() == 1 && geometry.bones[0].pivot[1] == 8, "current pivot");
    const auto &quad = geometry.bones[0].cubes[0].quads[0];
    expect(quad.positions[3] == 1 && quad.positions[7] == 1 && quad.normal[2] == 1,
           "indexed current vertices and normals");
    const auto &file = asset->animation_files[0];
    const auto &clip = file.clips[0];
    expect(file.first_person && clip.length == 2 && clip.loop == omm::ysm::LoopMode::loop,
           "current tick/second and first-person mapping");
    expect(clip.bones[0].position->keyframes[0].time == 0.5f && clip.bones[0].position->keyframes[0].post[2].value == 2,
           "current keyframe time and scalar broadcast");
    expect(clip.events.size() == 2 && clip.events[0].type == "timeline" && clip.events[1].type == "sound",
           "equal-time current event order");
    expect(clip.events[0].program->format == 7 &&
               clip.events[0].program->bytecode == omm::test::current::Bytes({0xFA, 0x00, 0x12}),
           "portable bytecode preserved, not evaluated");
    expect(clip.blend_weight_program->source == "q.blend", "portable source preserved");
    expect(asset->controller_files[0].controllers[0].states[0].entry_program->source == "v.enter=1;",
           "current controller entry program");

    auto error = error_value();
    omm_runtime_desc runtime_desc{};
    runtime_desc.struct_size = sizeof(runtime_desc);
    runtime_desc.version = OMM_RUNTIME_DESC_VERSION;
    runtime_desc.requested_abi_major = OMM_ABI_MAJOR;
    runtime_desc.requested_abi_minor = OMM_ABI_MINOR;
    omm_handle runtime = 0;
    expect(omm_runtime_create(&runtime_desc, &runtime, &error) == omm_ok, "current runtime");
    omm_ysm_parse_desc desc{};
    desc.struct_size = sizeof(desc);
    desc.version = OMM_YSM_PARSE_DESC_VERSION;
    desc.source_profile = omm_profile_ysm_current;
    omm_handle handle = 0;
    expect(omm_ysm_parse(runtime, {fixture.data(), fixture.size()}, &desc, &handle, &error) == omm_ok,
           "current C ABI parse");
    const std::string animations = section(handle, omm_section_animations);
    expect(animations.find("fa0012") != std::string::npos && animations.find("q.blend") != std::string::npos,
           "portable payloads cross public section boundary");
    for (uint32_t id = 1; id <= 6; ++id) {
        expect(section(handle, id) == omm::ysm::write_section(*asset, static_cast<omm::ysm::Section>(id)),
               "current serial and automatic-thread sections are identical");
    }
    expect(omm_asset_release(handle, &error) == omm_ok, "current release");
    omm_ysm_probe_result probe{};
    probe.struct_size = sizeof(probe);
    expect(omm_ysm_probe({fixture.data(), 8}, &probe, &error) == omm_ok && probe.format == omm_ysm_asset_container &&
               probe.parseable == 1,
           "truncated current header format detection");
    desc.source_profile = omm_profile_omp_compat;
    expect(omm_ysm_parse(runtime, {fixture.data(), fixture.size()}, &desc, &handle, &error) == omm_error_unsupported,
           "current container does not mix OMP profile");
    desc.source_profile = omm_profile_ysm_current;
    for (auto broken : {omm::test::current::container(true), omm::test::current::container(false, true),
                        omm::test::current::container(false, false, true)}) {
        expect(omm_ysm_parse(runtime, {broken.data(), broken.size()}, &desc, &handle, &error) == omm_error_format &&
                   handle == 0,
               "invalid index, hierarchy or blob-role rejected atomically");
    }
    for (size_t length : {8u, 10u, 74u}) {
        expect(omm_ysm_parse(runtime, {fixture.data(), length}, &desc, &handle, &error) == omm_error_format &&
                   handle == 0,
               "truncated current container rejected");
    }
    auto unsupported = fixture;
    unsupported[13] = 2;
    expect(omm_ysm_parse(runtime, {unsupported.data(), unsupported.size()}, &desc, &handle, &error) ==
               omm_error_unsupported,
           "unsupported container version reported separately");
    expect(omm_runtime_destroy(runtime, &error) == omm_ok, "current error paths release all temporary assets");
}

} // namespace

int main(int argc, char **argv) {
    if (argc == 3 && std::string(argv[1]) == "--write-fixture") {
        const auto bytes = omm::test::current::container();
        std::ofstream output(argv[2], std::ios::binary);
        output.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        output.close();
        if (!output) {
            return 1;
        }
        std::printf("Wrote own current-schema fixture: %zu bytes\n", bytes.size());
        return 0;
    }
    test_current_asset();
    if (failures != 0) {
        return 1;
    }
    std::printf("current_parser_test: geometry, programs, animation, controllers and rejection paths passed\n");
    return 0;
}
