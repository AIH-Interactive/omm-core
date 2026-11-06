// Native counterpart of tools/m2/omp-harness/OmpDecodeProbe.java: decodes encrypted
// .ysm files in memory and prints only sizes and SHA-256 digests, in the same
// tab-separated format, so the two outputs can be diffed line by line.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "../support/binary_digest.hpp"
#include "../support/sha256.hpp"
#include "abi/error.hpp"
#include "ysmparser/ysgp.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

std::vector<uint8_t> read_file(const std::filesystem::path &path) {
    std::ifstream stream(path, std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

void probe(const std::filesystem::path &path) {
    const std::string name = reinterpret_cast<const char *>(path.filename().u8string().c_str());
    const std::vector<uint8_t> bytes = read_file(path);
    omm::ysm::Budget budget{omm::ysm::Limits{}};
    try {
        const omm::ysm::YsgpKind kind = omm::ysm::probe_ysgp(bytes);
        if (kind == omm::ysm::YsgpKind::legacy_v1 || kind == omm::ysm::YsgpKind::legacy_v2) {
            std::printf("file\t%s\tlegacy-v%d\n", name.c_str(), kind == omm::ysm::YsgpKind::legacy_v1 ? 1 : 2);
            const omm::ysm::ResourceSet files = omm::ysm::decode_legacy(bytes, budget);
            for (const omm::ysm::Resource &resource : files.entries()) {
                std::printf("entry\t%s\t%s\t%zu\t%s\n", name.c_str(), resource.path.c_str(), resource.bytes.size(),
                            omm::test::sha256_hex(resource.bytes).c_str());
            }
        } else {
            std::printf("file\t%s\tv3\n", name.c_str());
            const std::vector<uint8_t> payload = omm::ysm::decode_v3(bytes, budget);
            const int format_dword = payload.size() >= 4
                                         ? static_cast<int>(uint32_t{payload[0]} | (uint32_t{payload[1]} << 8) |
                                                            (uint32_t{payload[2]} << 16) | (uint32_t{payload[3]} << 24))
                                         : -1;
            std::printf("payload\t%s\t%zu\t%s\t%d\n", name.c_str(), payload.size(),
                        omm::test::sha256_hex(payload).c_str(), format_dword);
            try {
                const omm::ysm::binary::Model model = omm::ysm::binary::read_payload(payload, budget);
                std::printf("binary\t%s\t%d\t%s\n", name.c_str(), model.format_version,
                            omm::test::BinaryDigest::of(model).c_str());
            } catch (const omm::abi::Failure &failure) {
                std::printf("binary\t%s\t%d\tERROR %.*s\n", name.c_str(), format_dword,
                            static_cast<int>(failure.message().size()), failure.message().data());
            }
        }
    } catch (const omm::abi::Failure &failure) {
        std::printf("error\t%s\t%.*s\n", name.c_str(), static_cast<int>(failure.message().size()),
                    failure.message().data());
    }
}

} // namespace

#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
    SetConsoleOutputCP(CP_UTF8);
    for (int index = 1; index < argc; ++index) {
        probe(std::filesystem::path(argv[index]));
    }
    return 0;
}
#else
int main(int argc, char **argv) {
    for (int index = 1; index < argc; ++index) {
        probe(std::filesystem::path(argv[index]));
    }
    return 0;
}
#endif
