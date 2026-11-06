// Diagnostic stage timings for one encrypted model; prints no decoded model data.
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "ysmparser/binary_mapper.hpp"
#include "ysmparser/manifest.hpp"
#include "ysmparser/normalize.hpp"
#include "ysmparser/parse.hpp"
#include "ysmparser/ysgp.hpp"

namespace {

using Clock = std::chrono::steady_clock;
using namespace omm::ysm;

double elapsed(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

int run(const std::filesystem::path &path, uint32_t workers) {
    std::ifstream input(path, std::ios::binary);
    const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    Limits limits;
    limits.worker_count = workers;
    for (unsigned int iteration = 0; iteration < 3; ++iteration) {
        Budget budget(limits);
        ResourceSet resources;
        Manifest manifest;
        double read_ms = 0;
        double map_ms = 0;
        const auto start = Clock::now();
        double decode_ms = 0;
        if (probe_ysgp(bytes) == YsgpKind::v3) {
            const auto payload = decode_v3(bytes, budget);
            decode_ms = elapsed(start);
            auto stage = Clock::now();
            const auto model = binary::read_payload(payload, budget);
            read_ms = elapsed(stage);
            stage = Clock::now();
            auto mapping = map_binary(model, "probe", budget);
            map_ms = elapsed(stage);
            resources = std::move(mapping.resources);
            manifest = std::move(mapping.manifest);
        } else {
            resources = decode_legacy(bytes, budget);
            decode_ms = elapsed(start);
            auto stage = Clock::now();
            std::string metadata;
            manifest = read_manifest(resources, "probe", metadata);
            read_ms = elapsed(stage);
            stage = Clock::now();
            for (const std::string &geometry : {manifest.main_model, manifest.arm_model}) {
                if (!geometry.empty()) {
                    std::vector<std::string> diagnostics;
                    auto document = parse_resource_json(resources, geometry, diagnostics);
                    if (!document) {
                        return 1;
                    }
                    parse_geometry(document->root(), geometry);
                }
            }
            map_ms = elapsed(stage);
        }
        double json_ms = 0;
        double animations_ms = 0;
        for (const std::string &animation : manifest.animations) {
            auto stage = Clock::now();
            std::vector<std::string> diagnostics;
            auto document = parse_resource_json(resources, animation, diagnostics);
            json_ms += elapsed(stage);
            if (!document) {
                return 1;
            }
            stage = Clock::now();
            parse_animations(document->root(), animation, manifest.merge_multiline_expressions, workers);
            animations_ms += elapsed(stage);
        }
        auto stage = Clock::now();
        auto asset = parse_container(bytes, "probe", limits);
        const double total_ms = elapsed(stage);
        stage = Clock::now();
        const auto section = write_section(*asset, Section::animations);
        std::printf("threads=%u round=%u decode=%.2f read=%.2f map_or_geometry=%.2f animation_json=%.2f "
                    "animation_normalize=%.2f total_parse=%.2f animation_section=%.2f section_bytes=%zu\n",
                    workers, iteration, decode_ms, read_ms, map_ms, json_ms, animations_ms, total_ms, elapsed(stage),
                    section.size());
    }
    return 0;
}

} // namespace

#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
#else
int main(int argc, char **argv) {
#endif
    if (argc != 3) {
        std::fprintf(stderr, "usage: omm_ysm_stage_probe <encrypted-model> <threads>\n");
        return 2;
    }
    return run(std::filesystem::path(argv[1]), static_cast<uint32_t>(std::stoul(argv[2])));
}
