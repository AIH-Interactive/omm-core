// Source → Asset orchestration and section serialization.
#pragma once

#include <memory>
#include <span>
#include <string>

#include "ysmparser/asset.hpp"

namespace omm::ysm {

enum class SourceFormat {
    unknown,
    raw,
    zip,
    legacy_v1,
    legacy_v2,
    legacy_v3,
    asset_container,
};

// Header-only identification (no decryption, no decompression).
SourceFormat probe(std::span<const uint8_t> input) noexcept;

// Full container bytes (ZIP or YSGP). display_name is the fallback model name.
std::unique_ptr<Asset> parse_container(std::span<const uint8_t> input, const std::string &display_name,
                                       const Limits &limits);

// Raw directory supplied as host entries.
std::unique_ptr<Asset> parse_entries(std::span<const EntryInput> entries, const std::string &display_name,
                                     const Limits &limits);

enum class Section {
    metadata = 1,
    animations = 2,
    controllers = 3,
    expressions = 4,
    resources = 5,
    diagnostics = 6,
};

// UTF-8 JSON, schema_version 1. Throws on an unknown section.
std::string write_section(const Asset &asset, Section section);

} // namespace omm::ysm
