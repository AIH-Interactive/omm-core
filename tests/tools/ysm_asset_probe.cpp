// Parses model samples through the public C ABI and prints a one-line summary per sample
// (format, counts, section sizes, first diagnostics). Nothing decoded is written to disk.
//
// Usage: omm_ysm_asset_probe <file-or-directory>...
//   directories are passed as raw entries (omm_ysm_parse_entries), files as containers.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "omm/omm.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

std::vector<uint8_t> read_file(const std::filesystem::path &path) {
    std::ifstream stream(path, std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

std::string utf8(const std::filesystem::path &path) {
    const auto text = path.u8string();
    return std::string(reinterpret_cast<const char *>(text.data()), text.size());
}

omm_error new_error() {
    omm_error error{};
    error.struct_size = sizeof(error);
    return error;
}

const char *format_name(uint32_t format) {
    switch (format) {
    case omm_ysm_raw:
        return "raw";
    case omm_ysm_zip:
        return "zip";
    case omm_ysm_legacy_v1:
        return "ysgp-v1";
    case omm_ysm_legacy_v2:
        return "ysgp-v2";
    case omm_ysm_legacy_v3:
        return "ysgp-v3";
    case omm_ysm_asset_container:
        return "current-container";
    default:
        return "unknown";
    }
}

std::string read_section(omm_handle asset, uint32_t section) {
    omm_error error = new_error();
    uint64_t required = 0;
    if (omm_asset_read_section(asset, section, omm_mut_bytes{nullptr, 0}, &required, &error) != omm_ok) {
        return std::string();
    }
    std::string text(static_cast<size_t>(required), '\0');
    omm_asset_read_section(asset, section, omm_mut_bytes{reinterpret_cast<uint8_t *>(text.data()), required}, &required,
                           &error);
    return text;
}

void probe(omm_handle runtime, const std::filesystem::path &path) {
    const std::string name = utf8(path.filename());
    omm_error error = new_error();
    omm_handle asset = 0;
    omm_ysm_parse_desc desc{};
    desc.struct_size = sizeof(desc);
    desc.version = OMM_YSM_PARSE_DESC_VERSION;
    desc.display_name = omm_bytes{reinterpret_cast<const uint8_t *>(name.data()), name.size()};

    const auto started = std::chrono::steady_clock::now();
    omm_status status;
    if (std::filesystem::is_directory(path)) {
        std::vector<std::filesystem::path> files;
        for (const auto &entry : std::filesystem::recursive_directory_iterator(path)) {
            if (entry.is_regular_file()) {
                files.push_back(entry.path());
            }
        }
        std::vector<std::vector<uint8_t>> contents;
        std::vector<std::string> names;
        contents.reserve(files.size());
        names.reserve(files.size());
        for (const auto &file : files) {
            names.push_back(utf8(std::filesystem::relative(file, path)));
            contents.push_back(read_file(file));
        }
        std::vector<omm_ysm_entry> entries;
        for (size_t index = 0; index < files.size(); ++index) {
            entries.push_back(
                omm_ysm_entry{omm_bytes{reinterpret_cast<const uint8_t *>(names[index].data()), names[index].size()},
                              omm_bytes{contents[index].data(), contents[index].size()}});
        }
        status = omm_ysm_parse_entries(runtime, entries.data(), static_cast<uint32_t>(entries.size()), &desc, &asset,
                                       &error);
    } else {
        const std::vector<uint8_t> bytes = read_file(path);
        status = omm_ysm_parse(runtime, omm_bytes{bytes.data(), bytes.size()}, &desc, &asset, &error);
    }
    const double milliseconds =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    if (status != omm_ok) {
        std::printf("FAIL\t%s\tstatus=%d\t%s\n", name.c_str(), status, error.message);
        return;
    }
    omm_asset_info info{};
    info.struct_size = sizeof(info);
    omm_asset_get_info(asset, &info, &error);
    size_t section_bytes = 0;
    for (uint32_t section = omm_section_metadata; section <= omm_section_diagnostics; ++section) {
        section_bytes += read_section(asset, section).size();
    }
    std::printf("OK\t%s\t%s/%u\tgeo=%u bones=%u quads=%u anim_files=%u clips=%u controllers=%u res=%u diag=%u "
                "retained=%llu sections=%zu\t%.1f ms\n",
                name.c_str(), format_name(info.source_format), info.source_version, info.geometry_count,
                info.bone_count, info.quad_count, info.animation_file_count, info.clip_count, info.controller_count,
                info.resource_count, info.diagnostic_count, static_cast<unsigned long long>(info.retained_bytes),
                section_bytes, milliseconds);
    if (info.diagnostic_count != 0) {
        std::printf("DIAG\t%s\t%s\n", name.c_str(), read_section(asset, omm_section_diagnostics).c_str());
    }
    omm_asset_release(asset, &error);
}

int run(const std::vector<std::filesystem::path> &paths) {
    omm_error error = new_error();
    omm_runtime_desc desc{};
    desc.struct_size = sizeof(desc);
    desc.version = OMM_RUNTIME_DESC_VERSION;
    desc.requested_abi_major = OMM_ABI_MAJOR;
    desc.requested_abi_minor = OMM_ABI_MINOR;
    omm_handle runtime = 0;
    if (omm_runtime_create(&desc, &runtime, &error) != omm_ok) {
        std::fprintf(stderr, "runtime: %s\n", error.message);
        return 2;
    }
    for (const auto &path : paths) {
        probe(runtime, path);
    }
    if (omm_runtime_destroy(runtime, &error) != omm_ok) {
        std::fprintf(stderr, "runtime destroy: %s\n", error.message);
        return 3;
    }
    return 0;
}

} // namespace

#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
    SetConsoleOutputCP(CP_UTF8);
    std::vector<std::filesystem::path> paths(argv + 1, argv + argc);
    return run(paths);
}
#else
int main(int argc, char **argv) {
    std::vector<std::filesystem::path> paths(argv + 1, argv + argc);
    return run(paths);
}
#endif
