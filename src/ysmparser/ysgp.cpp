#include "ysmparser/ysgp.hpp"

#include <algorithm>
#include <array>
#include <string>

#include "miniz.h"
#include "ysmparser/crypto.hpp"
#include "zstd.h"

namespace omm::ysm {

namespace {

constexpr uint64_t kFileVerification = 0x9E5599DB80C67C29ull;
constexpr uint64_t kResourceVerification = 0xA62B1A2C43842BC3ull;
constexpr uint64_t kKeyDerivation = 0xD017CBBA7B5D3581ull;

size_t magic_offset(std::span<const uint8_t> source) noexcept {
    const size_t offset = source.size() >= 3 && source[0] == 0xEF && source[1] == 0xBB && source[2] == 0xBF ? 3 : 0;
    if (source.size() >= offset + 4 && source[offset] == 'Y' && source[offset + 1] == 'S' &&
        source[offset + 2] == 'G' && source[offset + 3] == 'P') {
        return offset;
    }
    return SIZE_MAX;
}

uint32_t read_u32_le(const uint8_t *p) noexcept {
    return uint32_t{p[0]} | (uint32_t{p[1]} << 8) | (uint32_t{p[2]} << 16) | (uint32_t{p[3]} << 24);
}

uint64_t read_u64_le(const uint8_t *p) noexcept {
    uint64_t value = 0;
    for (int index = 7; index >= 0; --index) {
        value = (value << 8) | p[index];
    }
    return value;
}

// java.util.Base64.getDecoder(): standard alphabet, optional padding, no whitespace.
bool base64_decode(std::span<const uint8_t> input, std::vector<uint8_t> &output) {
    auto value_of = [](uint8_t c) -> int {
        if (c >= 'A' && c <= 'Z')
            return c - 'A';
        if (c >= 'a' && c <= 'z')
            return c - 'a' + 26;
        if (c >= '0' && c <= '9')
            return c - '0' + 52;
        if (c == '+')
            return 62;
        if (c == '/')
            return 63;
        return -1;
    };
    output.clear();
    uint32_t bits = 0;
    int bit_count = 0;
    size_t index = 0;
    size_t data_chars = 0;
    for (; index < input.size(); ++index) {
        if (input[index] == '=') {
            break;
        }
        const int value = value_of(input[index]);
        if (value < 0) {
            return false;
        }
        bits = (bits << 6) | static_cast<uint32_t>(value);
        bit_count += 6;
        ++data_chars;
        if (bit_count >= 8) {
            bit_count -= 8;
            output.push_back(static_cast<uint8_t>(bits >> bit_count));
        }
    }
    const size_t remainder = data_chars % 4;
    if (remainder == 1) {
        return false; // A single trailing sextet cannot encode a byte.
    }
    if (index < input.size()) {
        // Padding present: it must complete the final quantum exactly and end the input.
        const size_t expected_padding = remainder == 0 ? 0 : 4 - remainder;
        const size_t padding = input.size() - index;
        if (expected_padding == 0 || padding != expected_padding) {
            return false;
        }
        for (; index < input.size(); ++index) {
            if (input[index] != '=') {
                return false;
            }
        }
    }
    return true;
}

// zlib-wrapped inflate (java.util.zip.Inflater default), bounded, no trailing bytes.
std::vector<uint8_t> inflate_zlib(std::span<const uint8_t> compressed, Budget &budget, uint64_t offset) {
    std::vector<uint8_t> output;
    if (compressed.empty()) {
        return output;
    }
    mz_stream stream{};
    if (mz_inflateInit(&stream) != MZ_OK) {
        fail(DecodeCode::decompress_failed, "legacy/deflate", offset);
    }
    stream.next_in = compressed.data();
    stream.avail_in = static_cast<unsigned int>(compressed.size());
    std::array<uint8_t, 8192> buffer{};
    bool finished = false;
    try {
        while (!finished) {
            budget.check("legacy/deflate", offset);
            stream.next_out = buffer.data();
            stream.avail_out = static_cast<unsigned int>(buffer.size());
            const int result = mz_inflate(&stream, MZ_NO_FLUSH);
            const size_t produced = buffer.size() - stream.avail_out;
            budget.length(static_cast<int64_t>(output.size() + produced), budget.remaining_resource_bytes(),
                          "legacy/deflate", offset);
            output.insert(output.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(produced));
            if (result == MZ_STREAM_END) {
                finished = true;
            } else if (result != MZ_OK || (produced == 0 && stream.avail_in == 0)) {
                fail(DecodeCode::decompress_failed, "legacy/deflate", offset);
            }
        }
        if (stream.avail_in != 0) {
            fail(DecodeCode::decompress_failed, "legacy/deflate-trailing", offset);
        }
    } catch (...) {
        mz_inflateEnd(&stream);
        secure_zero(buffer.data(), buffer.size());
        secure_clear(output);
        throw;
    }
    mz_inflateEnd(&stream);
    secure_zero(buffer.data(), buffer.size());
    return output;
}

std::vector<uint8_t> aes_or_fail(std::span<const uint8_t> ciphertext, std::span<const uint8_t> key,
                                 std::span<const uint8_t> iv, uint64_t offset) {
    std::vector<uint8_t> plaintext;
    if (iv.size() != 16 ||
        !crypto::aes_cbc_decrypt(key, std::span<const uint8_t, 16>(iv.data(), 16), ciphertext, plaintext)) {
        fail(DecodeCode::decrypt_failed, "legacy/aes", offset);
    }
    return plaintext;
}

} // namespace

YsgpKind probe_ysgp(std::span<const uint8_t> source) noexcept {
    const size_t magic = magic_offset(source);
    if (magic == SIZE_MAX) {
        return YsgpKind::none;
    }
    if (magic == 0 && source.size() >= 8) {
        const uint32_t legacy =
            (uint32_t{source[4]} << 24) | (uint32_t{source[5]} << 16) | (uint32_t{source[6]} << 8) | source[7];
        if (legacy == 1) {
            return YsgpKind::legacy_v1;
        }
        if (legacy == 2) {
            return YsgpKind::legacy_v2;
        }
    }
    // Text header up to NUL, then a little-endian crypto dword.
    size_t end = magic + 4;
    while (end < source.size() && source[end] != 0) {
        if (end + 1 > Budget::kMaxHeaderBytes) {
            return YsgpKind::unknown_version;
        }
        ++end;
    }
    if (end + 5 > source.size()) {
        return YsgpKind::unknown_version;
    }
    return read_u32_le(source.data() + end + 1) == 3 ? YsgpKind::v3 : YsgpKind::unknown_version;
}

ResourceSet decode_legacy(std::span<const uint8_t> source, Budget &budget) {
    budget.length(static_cast<int64_t>(source.size()), budget.limits().file_bytes, "legacy/source", 0);
    ByteReader reader(source, budget, "legacy");
    if (reader.i32(Endian::big) != 0x59534750) {
        fail(DecodeCode::bad_header, "legacy", 0);
    }
    const int32_t version = reader.i32(Endian::big);
    if (version != 1 && version != 2) {
        fail(DecodeCode::unsupported_version, "legacy/crypto", 4);
    }
    const auto expected = reader.bytes(16);
    const auto actual = crypto::md5(source.subspan(24));
    if (!std::equal(expected.begin(), expected.end(), actual.begin())) {
        fail(DecodeCode::checksum_failed, "legacy/container", 8);
    }

    ResourceSet files;
    while (reader.remaining() > 0) {
        budget.check("legacy/resource", reader.offset());
        budget.begin_resource("legacy/resource-count", reader.offset());
        const uint32_t name_length =
            budget.length(reader.i32(Endian::big), Budget::kMaxStringBytes, "legacy/name", reader.offset());
        std::vector<uint8_t> name_bytes;
        {
            const auto raw = reader.bytes(name_length);
            name_bytes.assign(raw.begin(), raw.end());
        }
        if (version == 2) {
            std::vector<uint8_t> decoded;
            if (!base64_decode(name_bytes, decoded)) {
                fail(DecodeCode::invalid_data, "legacy/name", reader.offset());
            }
            name_bytes = std::move(decoded);
        }
        const std::string name(name_bytes.begin(), name_bytes.end());
        if (!is_valid_utf8(name)) {
            fail(DecodeCode::invalid_data, "legacy/utf8", reader.offset());
        }
        const auto normalized = normalize_relative(name);
        if (!normalized || normalized->empty()) {
            fail(DecodeCode::invalid_path, "legacy/name", reader.offset());
        }
        if (files.exists(*normalized)) {
            fail(DecodeCode::path_conflict, "legacy/name", reader.offset());
        }

        const uint32_t size =
            budget.length(reader.i32(Endian::big), budget.limits().file_bytes, "legacy/ciphertext", reader.offset());
        std::vector<uint8_t> key;
        std::vector<uint8_t> iv;
        std::vector<uint8_t> ciphertext;
        std::vector<uint8_t> compressed;
        try {
            if (version == 1) {
                const auto raw_key = reader.bytes(16);
                key.assign(raw_key.begin(), raw_key.end());
                const auto raw_iv = reader.bytes(16);
                iv.assign(raw_iv.begin(), raw_iv.end());
                const auto raw_cipher = reader.bytes(size);
                ciphertext.assign(raw_cipher.begin(), raw_cipher.end());
            } else {
                const uint32_t wrapped_size =
                    budget.length(reader.i32(Endian::big), 64, "legacy/wrapped-key", reader.offset());
                const auto wrapped = reader.bytes(wrapped_size);
                const auto raw_iv = reader.bytes(16);
                iv.assign(raw_iv.begin(), raw_iv.end());
                const auto raw_cipher = reader.bytes(size);
                ciphertext.assign(raw_cipher.begin(), raw_cipher.end());

                auto hash = crypto::md5(ciphertext);
                uint64_t seed = 0;
                for (uint8_t value : hash) {
                    seed = (seed << 8) + value;
                }
                std::array<uint8_t, 16> wrapping_key{};
                crypto::java_random_next_bytes(static_cast<int64_t>(seed), wrapping_key);
                secure_zero(hash.data(), hash.size());
                try {
                    key = aes_or_fail(wrapped, wrapping_key, iv, reader.offset());
                } catch (...) {
                    secure_zero(wrapping_key.data(), wrapping_key.size());
                    throw;
                }
                secure_zero(wrapping_key.data(), wrapping_key.size());
            }
            compressed = aes_or_fail(ciphertext, key, iv, reader.offset());
            std::vector<uint8_t> resource = inflate_zlib(compressed, budget, reader.offset());
            budget.resource(resource.size(), "legacy/resource", reader.offset());
            files.add(*normalized, std::move(resource), "legacy/name");
        } catch (...) {
            secure_clear(key);
            secure_clear(ciphertext);
            secure_clear(compressed);
            throw;
        }
        secure_clear(key);
        secure_clear(ciphertext);
        secure_clear(compressed);
    }
    if (files.size() == 0) {
        fail(DecodeCode::invalid_data, "legacy/empty", reader.offset());
    }
    return files;
}

std::vector<uint8_t> decode_v3(std::span<const uint8_t> source, Budget &budget) {
    budget.length(static_cast<int64_t>(source.size()), budget.limits().file_bytes, "v3/source", 0);
    if (source.size() < 77) {
        fail(DecodeCode::truncated, "v3/source", 0);
    }
    const size_t magic = magic_offset(source);
    if (magic == SIZE_MAX) {
        fail(DecodeCode::bad_header, "v3/magic", 0);
    }
    const size_t tail_offset = source.size() - 64;
    size_t header_end = magic + 4;
    while (header_end < tail_offset && source[header_end] != 0) {
        budget.length(static_cast<int64_t>(header_end + 1), Budget::kMaxHeaderBytes, "v3/text-header", header_end);
        ++header_end;
    }
    if (header_end >= tail_offset || tail_offset - header_end < 5) {
        fail(DecodeCode::truncated, "v3/text-header", header_end);
    }
    const size_t crypto_offset = header_end + 1;
    if (read_u32_le(source.data() + crypto_offset) != 3) {
        fail(DecodeCode::unsupported_version, "v3/crypto", crypto_offset);
    }
    const uint64_t expected = read_u64_le(source.data() + source.size() - 8);
    const uint64_t actual = crypto::city_hash64_with_seed(source.first(source.size() - 8), kFileVerification);
    budget.check("v3/integrity", tail_offset);
    if (actual != expected) {
        fail(DecodeCode::checksum_failed, "v3/file", tail_offset + 56);
    }

    const auto key = source.subspan(tail_offset, 32);
    const auto nonce = source.subspan(tail_offset + 32, 24);
    const auto key_nonce = source.subspan(tail_offset, 56);
    const size_t payload_offset = crypto_offset + 4;
    std::vector<uint8_t> payload(tail_offset - payload_offset);
    try {
        const uint64_t initial = crypto::city_hash64_with_seed(key_nonce, kResourceVerification);
        uint32_t chunk_size = static_cast<uint32_t>(((initial & 63u) | 64u) << 6);
        const int rounds = 10 * static_cast<int>(initial % 3u) + 10;
        {
            crypto::ChaCha cipher(std::span<const uint8_t, 32>(key.data(), 32),
                                  std::span<const uint8_t, 24>(nonce.data(), 24), rounds);
            size_t pointer = 0;
            while (pointer < payload.size()) {
                budget.check("v3/chacha", pointer);
                const size_t count = std::min<size_t>(chunk_size, payload.size() - pointer);
                cipher.process(source.data() + payload_offset + pointer, payload.data() + pointer, count);
                pointer += count;
                if (pointer < payload.size()) {
                    chunk_size = cipher.update(crypto::city_hash64_with_seed(
                        std::span<const uint8_t>(payload.data() + pointer - count, count), kResourceVerification));
                }
            }
        }
        {
            crypto::Mt19937_64 random(crypto::city_hash64_with_seed(key_nonce, kKeyDerivation));
            for (size_t pointer = 0; pointer < payload.size();) {
                if ((pointer & 8191u) == 0) {
                    budget.check("v3/xor", pointer);
                }
                const uint64_t stream = random.next();
                for (int index = 0; index < 8 && pointer < payload.size(); ++index, ++pointer) {
                    payload[pointer] ^= static_cast<uint8_t>(stream >> (index * 8));
                }
            }
        }
        ByteReader reader(payload, budget, "v3/padding");
        const uint32_t padding = (uint32_t{reader.u8()} | (uint32_t{reader.u8()} << 8)) & 1023u;
        reader.skip(padding);
        std::vector<uint8_t> result =
            decompress_ysm_zstd(std::span<const uint8_t>(payload.data() + reader.offset(), reader.remaining()), budget);
        secure_clear(payload);
        return result;
    } catch (...) {
        secure_clear(payload);
        throw;
    }
}

std::vector<uint8_t> decompress_ysm_zstd(std::span<const uint8_t> input, Budget &budget) {
    std::vector<uint8_t> frame(input.begin(), input.end());
    std::vector<uint8_t> output;
    ZSTD_DStream *stream = nullptr;
    try {
        // ---- Restore block headers (OMP YsmZstdDecoder.restoreBlockHeaders) ----
        ByteReader reader(frame, budget, "zstd/header");
        if (static_cast<uint32_t>(reader.i32(Endian::little)) != 0xFD2FB528u) {
            fail(DecodeCode::bad_header, "zstd/magic", 0);
        }
        const uint8_t descriptor = reader.u8();
        if ((descriptor & 0x18u) != 0) {
            fail(DecodeCode::invalid_data, "zstd/descriptor", 4);
        }
        const bool single_segment = (descriptor & 32u) != 0;
        const bool checksum = (descriptor & 4u) != 0;
        if (!single_segment) {
            const uint8_t window = reader.u8();
            const uint64_t base = 1ull << ((window >> 3) + 10);
            budget.length(static_cast<int64_t>(base + (base >> 3) * (window & 7u)),
                          budget.limits().total_resource_bytes, "zstd/window", reader.offset());
        }
        const uint32_t dictionary_flag = descriptor & 3u;
        const uint32_t dictionary_bytes = dictionary_flag == 0 ? 0 : 1u << (dictionary_flag - 1);
        reader.skip(dictionary_bytes);
        if (dictionary_bytes != 0) {
            fail(DecodeCode::unsupported_version, "zstd/dictionary", reader.offset());
        }
        const uint32_t size_flag = descriptor >> 6;
        const uint32_t size_bytes = size_flag == 0 ? (single_segment ? 1u : 0u) : 1u << size_flag;
        uint64_t content_size = 0;
        for (uint32_t index = 0; index < size_bytes; ++index) {
            content_size |= uint64_t{reader.u8()} << (index * 8);
        }
        if (size_flag == 1) {
            content_size += 256;
        }
        if (size_bytes != 0) {
            budget.length(static_cast<int64_t>(content_size), budget.limits().total_resource_bytes, "zstd/content-size",
                          reader.offset());
        }
        bool last;
        do {
            const uint64_t block_offset = reader.offset();
            const uint32_t first = reader.u8();
            const uint32_t second = reader.u8();
            const uint32_t third = reader.u8();
            last = (first & 128u) != 0;
            const uint32_t ysm_type = (first >> 5) & 3u;
            uint32_t type;
            switch (ysm_type) {
            case 0:
                type = 2;
                break;
            case 1:
                type = 1;
                break;
            case 3:
                type = 0;
                break;
            default:
                fail(DecodeCode::invalid_data, "zstd/block-type", block_offset);
            }
            const uint32_t block_size = (((first & 31u) << 16) | second | (third << 8)) ^ 0xD4E9u;
            budget.length(block_size, 128 * 1024, "zstd/block-size", block_offset);
            const uint32_t header = (last ? 1u : 0u) | (type << 1) | (block_size << 3);
            frame[block_offset] = static_cast<uint8_t>(header);
            frame[block_offset + 1] = static_cast<uint8_t>(header >> 8);
            frame[block_offset + 2] = static_cast<uint8_t>(header >> 16);
            reader.skip(type == 1 ? 1 : block_size);
        } while (!last);
        reader.skip(checksum ? 4 : 0);
        if (reader.remaining() != 0) {
            fail(DecodeCode::invalid_data, "zstd/trailing-data", reader.offset());
        }

        // ---- Decompress with the restored standard frame ----
        stream = ZSTD_createDStream();
        if (stream == nullptr || ZSTD_isError(ZSTD_initDStream(stream))) {
            throw std::bad_alloc();
        }
        ZSTD_inBuffer in{frame.data(), frame.size(), 0};
        std::vector<uint8_t> buffer(ZSTD_DStreamOutSize());
        size_t remaining_hint = 1;
        while (remaining_hint != 0) {
            budget.check("zstd/decode", output.size());
            ZSTD_outBuffer out{buffer.data(), buffer.size(), 0};
            remaining_hint = ZSTD_decompressStream(stream, &out, &in);
            if (ZSTD_isError(remaining_hint)) {
                fail(DecodeCode::decompress_failed, "zstd/decode", in.pos);
            }
            budget.length(static_cast<int64_t>(output.size() + out.pos), budget.limits().total_resource_bytes,
                          "zstd/output", output.size());
            output.insert(output.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(out.pos));
            if (remaining_hint != 0 && in.pos == in.size && out.pos == 0) {
                fail(DecodeCode::decompress_failed, "zstd/truncated", in.pos);
            }
        }
        secure_clear(buffer);
    } catch (...) {
        if (stream != nullptr) {
            ZSTD_freeDStream(stream);
        }
        secure_clear(frame);
        secure_clear(output);
        throw;
    }
    ZSTD_freeDStream(stream);
    secure_clear(frame);
    return output;
}

} // namespace omm::ysm
