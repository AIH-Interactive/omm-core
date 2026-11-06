#include "ysmparser/current_container.hpp"

#include <algorithm>
#include <limits>

#include "ysmparser/crypto.hpp"
#include "zstd.h"

namespace omm::ysm {
namespace {

constexpr std::array<uint8_t, 10> kMagic{0xEF, 0xBB, 0xBF, 'Y', 'S', 'G', 'P', '2', '\n', '\n'};

uint16_t u16(ByteReader &reader) {
    const uint16_t low = reader.u8();
    return static_cast<uint16_t>(low | (static_cast<uint16_t>(reader.u8()) << 8));
}

std::string string_bytes(std::span<const uint8_t> data) {
    return std::string(reinterpret_cast<const char *>(data.data()), data.size());
}

std::string short_string(ByteReader &reader) {
    const uint8_t size = reader.u8();
    if (size > 127) {
        fail(DecodeCode::invalid_data, "container/short-string", reader.offset());
    }
    return string_bytes(reader.bytes(size));
}

std::string fixed_string(ByteReader &reader, size_t size) {
    auto bytes = reader.bytes(size);
    while (!bytes.empty() && bytes.back() == 0) {
        bytes = bytes.first(bytes.size() - 1);
    }
    return string_bytes(bytes);
}

struct Chunk {
    std::string type;
    std::string encoding;
    uint32_t size = 0;
    uint32_t decoded_size = 0;
    uint32_t alignment_shift = 0;
    uint64_t offset = 0;
    std::array<uint8_t, 32> hash{};
};

} // namespace

bool is_current_container(std::span<const uint8_t> input) noexcept {
    // BOM + YSGP2 already identifies the format, even when the rest of its header is truncated.
    return input.size() >= 8 && std::equal(kMagic.begin(), kMagic.begin() + 8, input.begin());
}

CurrentContainer decode_current_container(std::span<const uint8_t> input, Budget &budget) {
    budget.length(static_cast<int64_t>(input.size()), std::min<uint64_t>(budget.limits().file_bytes, 128ull << 20),
                  "container/input", 0);
    if (input.size() < kMagic.size()) {
        fail(DecodeCode::truncated, "container/magic", 0);
    }
    if (!std::equal(kMagic.begin(), kMagic.end(), input.begin())) {
        fail(DecodeCode::bad_header, "container/magic", 0);
    }
    ByteReader reader(input, budget, "container/header");
    reader.skip(kMagic.size());
    size_t summary_size = 0;
    while (reader.u8() != 0) {
        if (++summary_size > 16384) {
            fail(DecodeCode::resource_limit, "container/summary", reader.offset());
        }
    }
    const uint64_t header_offset = reader.offset();
    const uint16_t header_size = u16(reader);
    if (header_size < 64 || header_size > input.size() - header_offset) {
        fail(DecodeCode::invalid_data, "container/header-size", header_offset);
    }
    const uint8_t major = reader.u8();
    const uint16_t minor = u16(reader);
    const uint16_t patch = u16(reader);
    const std::string qualifier = fixed_string(reader, 13);
    if (major != 0 || minor != 1 || patch != 0 || qualifier != "unstable") {
        fail(DecodeCode::unsupported_version, "container/version", header_offset);
    }

    CurrentContainer container;
    container.schema = fixed_string(reader, 32);
    const uint16_t property_count = u16(reader);
    const uint16_t chunk_count = u16(reader);
    const int32_t property_bytes = reader.i32(Endian::little);
    const int32_t table_bytes = reader.i32(Endian::little);
    if (property_count > 32767 || chunk_count == 0 || chunk_count > 32767 || property_bytes < 0 || table_bytes < 0) {
        fail(DecodeCode::invalid_data, "container/counts", reader.offset());
    }
    reader.skip(header_offset + header_size - reader.offset());
    ByteReader properties(reader.bytes(static_cast<uint32_t>(property_bytes)), budget, "container/properties");
    for (uint16_t index = 0; index < property_count; ++index) {
        const uint16_t type = u16(properties);
        if (type > 32767 || !container.properties.emplace(type, short_string(properties)).second) {
            fail(DecodeCode::invalid_data, "container/property-type", properties.offset());
        }
    }
    if (properties.remaining() != 0) {
        fail(DecodeCode::invalid_data, "container/property-trailing", properties.offset());
    }
    ByteReader table(reader.bytes(static_cast<uint32_t>(table_bytes)), budget, "container/chunk-table");
    const uint64_t data_offset = reader.offset();
    uint64_t next_offset = data_offset;
    std::vector<Chunk> chunks;
    chunks.reserve(chunk_count);
    for (uint16_t index = 0; index < chunk_count; ++index) {
        Chunk chunk;
        chunk.type = short_string(table);
        chunk.encoding = short_string(table);
        const int32_t stored_size = table.i32(Endian::little);
        const int32_t decoded_size = table.i32(Endian::little);
        table.i32(Endian::little); // Reserved storage flags do not grant resource authority.
        chunk.alignment_shift = table.u8();
        if (stored_size < 0 || decoded_size < 0 || chunk.alignment_shift > 8) {
            fail(DecodeCode::invalid_data, "container/chunk-size", table.offset());
        }
        chunk.size = static_cast<uint32_t>(stored_size);
        chunk.decoded_size = static_cast<uint32_t>(decoded_size);
        const auto hash = table.bytes(chunk.hash.size());
        std::copy(hash.begin(), hash.end(), chunk.hash.begin());
        const uint64_t alignment = uint64_t{1} << chunk.alignment_shift;
        chunk.offset = next_offset + (alignment - next_offset % alignment) % alignment;
        if (chunk.offset > input.size() || chunk.size > input.size() - chunk.offset) {
            fail(DecodeCode::truncated, "container/chunk-range", chunk.offset);
        }
        next_offset = chunk.offset + chunk.size;
        if (!container.encodings.emplace(chunk.type, chunk.encoding).second) {
            fail(DecodeCode::path_conflict, "container/duplicate-chunk", table.offset());
        }
        chunks.push_back(std::move(chunk));
    }
    if (table.remaining() != 0 || next_offset != input.size()) {
        fail(DecodeCode::invalid_data, "container/trailing-data", next_offset);
    }

    const Chunk &verification = chunks.front();
    if (verification.type != "verification" || verification.alignment_shift != 0 || verification.size < 34 ||
        std::any_of(verification.hash.begin(), verification.hash.end(), [](uint8_t value) { return value != 0; })) {
        fail(DecodeCode::invalid_data, "container/verification", verification.offset);
    }
    if (verification.encoding != "BLAKE3") {
        fail(DecodeCode::unsupported_version, "container/verification-method", verification.offset);
    }
    const auto verification_data = input.subspan(static_cast<size_t>(verification.offset), verification.size);
    const uint32_t signature_length = verification_data[32] | (static_cast<uint32_t>(verification_data[33]) << 8);
    if (verification.size != 34 + signature_length) {
        fail(DecodeCode::invalid_data, "container/signature-length", verification.offset);
    }
    container.container_id = crypto::blake3(input.first(static_cast<size_t>(data_offset)));
    if (!std::equal(container.container_id.begin(), container.container_id.end(), verification_data.begin())) {
        fail(DecodeCode::checksum_failed, "container/header-hash", verification.offset);
    }
    for (size_t index = 1; index < chunks.size(); ++index) {
        const Chunk &chunk = chunks[index];
        budget.begin_resource("container/chunk-count", chunk.offset);
        const uint64_t logical_size = chunk.encoding == "zstd" ? chunk.decoded_size : chunk.size;
        budget.resource(logical_size, "container/decoded-size", chunk.offset);
        const auto stored = input.subspan(static_cast<size_t>(chunk.offset), chunk.size);
        std::vector<uint8_t> logical(static_cast<size_t>(logical_size));
        if (chunk.encoding == "zstd") {
            const size_t decoded = ZSTD_decompress(logical.data(), logical.size(), stored.data(), stored.size());
            if (ZSTD_isError(decoded) || decoded != logical.size()) {
                fail(DecodeCode::decompress_failed, "container/zstd", chunk.offset);
            }
        } else {
            std::copy(stored.begin(), stored.end(), logical.begin());
        }
        if (crypto::blake3(logical) != chunk.hash) {
            fail(DecodeCode::checksum_failed, "container/chunk-hash", chunk.offset);
        }
        container.chunks.add(chunk.type, std::move(logical), "container/chunk-path");
    }
    return container;
}

} // namespace omm::ysm
