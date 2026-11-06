// Read-only YSGP container decoders (ports of OMP YsmLegacyDecoder, YsmV3Decoder and
// YsmZstdDecoder). No writer, cache or network protocol is implemented.
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "ysmparser/decode.hpp"
#include "ysmparser/resource_set.hpp"

namespace omm::ysm {

enum class YsgpKind {
    none,
    legacy_v1,
    legacy_v2,
    v3,
    unknown_version,
};

// Inspects only the header (no decryption). Optional UTF-8 BOM before "YSGP".
YsgpKind probe_ysgp(std::span<const uint8_t> source) noexcept;

// Crypto 1/2: decrypted, inflated resource files.
ResourceSet decode_legacy(std::span<const uint8_t> source, Budget &budget);

// Crypto 3: decrypted and zstd-decompressed binary model payload.
std::vector<uint8_t> decode_v3(std::span<const uint8_t> source, Budget &budget);

// Restores YSM-obfuscated zstd block headers in place, then decompresses within the budget.
std::vector<uint8_t> decompress_ysm_zstd(std::span<const uint8_t> frame, Budget &budget);

} // namespace omm::ysm
