// Decode diagnostics, budgets and bounded byte reading for the native ysmparser.
//
// Semantics follow OMP's YsmDecodeException / YsmDecodeBudget / YsmByteReader
// (refs/OMP/OMP_26.3/.../avatar/ysm/crypto): every length is checked before a
// slice or allocation, totals are shared across nested layers, and diagnostics
// carry codes and offsets but never decrypted content.
#pragma once

#include <chrono>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "abi/error.hpp"
#include "omm/omm.h"

namespace omm::ysm {

enum class DecodeCode {
    bad_header,
    truncated,
    checksum_failed,
    unsupported_version,
    decrypt_failed,
    decompress_failed,
    resource_limit,
    invalid_path,
    path_conflict,
    invalid_data,
};

const char *decode_code_name(DecodeCode code) noexcept;

// Throws abi::Failure with "CODE: stage=<stage> offset=<offset>" and the mapped omm status.
[[noreturn]] void fail(DecodeCode code, std::string_view stage, uint64_t offset);

struct Limits {
    uint32_t worker_count = 1;
    uint32_t nesting_limit = 0;
    uint64_t file_bytes = 64ull * 1024 * 1024;
    uint64_t single_resource_bytes = 32ull * 1024 * 1024;
    uint64_t total_resource_bytes = 256ull * 1024 * 1024;
    uint32_t resource_count = 4096;
    uint32_t timeout_milliseconds = 60000;
};

class Budget {
public:
    static constexpr uint64_t kMaxHeaderBytes = 65536;
    static constexpr uint64_t kMaxStringBytes = 1048576;
    static constexpr uint64_t kMaxTexturePixels = 16777216;

    explicit Budget(const Limits &limits);

    const Limits &limits() const noexcept {
        return limits_;
    }

    // Deadline check.
    void check(std::string_view stage, uint64_t offset) const;

    // Validates 0 <= length <= max (and <= INT32_MAX, as in the reference); returns it.
    uint32_t length(int64_t length, uint64_t max, std::string_view stage, uint64_t offset) const;

    void resource(uint64_t bytes, std::string_view stage, uint64_t offset);
    void begin_resource(std::string_view stage, uint64_t offset);
    uint64_t remaining_resource_bytes() const noexcept;

private:
    Limits limits_;
    std::chrono::steady_clock::time_point started_;
    // The deadline is checked against the clock every kClockStride calls; reading the clock on
    // every byte dominated decoding time. Granularity, not semantics, changes.
    static constexpr uint32_t kClockStride = 1024;
    mutable uint32_t checks_until_clock_ = 0;
    uint64_t resource_bytes_ = 0;
    uint32_t resource_count_ = 0;
};

enum class Endian { little, big };

class ByteReader {
public:
    ByteReader(std::span<const uint8_t> data, const Budget &budget, std::string stage)
        : data_(data), budget_(budget), stage_(std::move(stage)) {}

    uint64_t offset() const noexcept {
        return offset_;
    }

    uint64_t remaining() const noexcept {
        return data_.size() - offset_;
    }

    const std::string &stage() const noexcept {
        return stage_;
    }

    void require(uint64_t bytes) const;
    uint8_t u8();
    int32_t i32(Endian endian);
    int64_t i64(Endian endian);
    float f32_finite();
    float f32_raw();
    int32_t var_int();
    int64_t var_long();
    std::span<const uint8_t> bytes(uint64_t length);
    void skip(uint64_t length);
    // varint length + strict UTF-8.
    std::string string();

private:
    std::span<const uint8_t> data_;
    const Budget &budget_;
    std::string stage_;
    uint64_t offset_ = 0;
};

// Strict UTF-8 validation (no overlong forms, no surrogates), matching Java's REPORT decoder.
bool is_valid_utf8(std::string_view text) noexcept;

// Overwrites sensitive buffers before release.
void secure_zero(void *data, size_t size) noexcept;

template <typename Container>
void secure_clear(Container &container) noexcept {
    if (!container.empty()) {
        secure_zero(container.data(), container.size() * sizeof(container[0]));
    }
    container.clear();
}

} // namespace omm::ysm
