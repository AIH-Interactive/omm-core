#include "ysmparser/decode.hpp"

#include <limits>

namespace omm::ysm {

const char *decode_code_name(DecodeCode code) noexcept {
    switch (code) {
    case DecodeCode::bad_header:
        return "BAD_HEADER";
    case DecodeCode::truncated:
        return "TRUNCATED";
    case DecodeCode::checksum_failed:
        return "CHECKSUM_FAILED";
    case DecodeCode::unsupported_version:
        return "UNSUPPORTED_VERSION";
    case DecodeCode::decrypt_failed:
        return "DECRYPT_FAILED";
    case DecodeCode::decompress_failed:
        return "DECOMPRESS_FAILED";
    case DecodeCode::resource_limit:
        return "RESOURCE_LIMIT";
    case DecodeCode::invalid_path:
        return "INVALID_PATH";
    case DecodeCode::path_conflict:
        return "PATH_CONFLICT";
    case DecodeCode::invalid_data:
        return "INVALID_DATA";
    }
    return "INVALID_DATA";
}

void fail(DecodeCode code, std::string_view stage, uint64_t offset) {
    omm_status status = omm_error_format;
    if (code == DecodeCode::unsupported_version) {
        status = omm_error_unsupported;
    } else if (code == DecodeCode::resource_limit) {
        status = omm_error_limit;
    }
    std::string message = decode_code_name(code);
    message += ": stage=";
    message += stage;
    message += " offset=";
    message += std::to_string(offset);
    throw abi::Failure(status, omm_stage_parse, std::move(message), offset);
}

Budget::Budget(const Limits &limits) : limits_(limits), started_(std::chrono::steady_clock::now()) {}

void Budget::check(std::string_view stage, uint64_t offset) const {
    if (checks_until_clock_ != 0) {
        --checks_until_clock_;
        return;
    }
    checks_until_clock_ = kClockStride;
    const auto elapsed = std::chrono::steady_clock::now() - started_;
    if (elapsed > std::chrono::milliseconds(limits_.timeout_milliseconds)) {
        fail(DecodeCode::resource_limit, std::string(stage) + "/deadline", offset);
    }
}

uint32_t Budget::length(int64_t length, uint64_t max, std::string_view stage, uint64_t offset) const {
    check(stage, offset);
    if (length < 0 || static_cast<uint64_t>(length) > max || length > std::numeric_limits<int32_t>::max()) {
        fail(DecodeCode::resource_limit, stage, offset);
    }
    return static_cast<uint32_t>(length);
}

void Budget::resource(uint64_t bytes, std::string_view stage, uint64_t offset) {
    length(static_cast<int64_t>(bytes), limits_.single_resource_bytes, stage, offset);
    // Unsigned form of the reference's signed check: bytes > total must not wrap around.
    if (bytes > limits_.total_resource_bytes || resource_bytes_ > limits_.total_resource_bytes - bytes) {
        fail(DecodeCode::resource_limit, stage, offset);
    }
    resource_bytes_ += bytes;
}

void Budget::begin_resource(std::string_view stage, uint64_t offset) {
    check(stage, offset);
    if (++resource_count_ > limits_.resource_count) {
        fail(DecodeCode::resource_limit, stage, offset);
    }
}

uint64_t Budget::remaining_resource_bytes() const noexcept {
    const uint64_t total_left = limits_.total_resource_bytes - resource_bytes_;
    return total_left < limits_.single_resource_bytes ? total_left : limits_.single_resource_bytes;
}

void ByteReader::require(uint64_t bytes) const {
    budget_.check(stage_, offset_);
    if (bytes > remaining()) {
        fail(DecodeCode::truncated, stage_, offset_);
    }
}

uint8_t ByteReader::u8() {
    require(1);
    return data_[offset_++];
}

int32_t ByteReader::i32(Endian endian) {
    require(4);
    const uint8_t *p = data_.data() + offset_;
    uint32_t value;
    if (endian == Endian::big) {
        value = (uint32_t{p[0]} << 24) | (uint32_t{p[1]} << 16) | (uint32_t{p[2]} << 8) | p[3];
    } else {
        value = (uint32_t{p[3]} << 24) | (uint32_t{p[2]} << 16) | (uint32_t{p[1]} << 8) | p[0];
    }
    offset_ += 4;
    return static_cast<int32_t>(value);
}

int64_t ByteReader::i64(Endian endian) {
    require(8);
    const uint8_t *p = data_.data() + offset_;
    uint64_t value = 0;
    for (int index = 0; index < 8; ++index) {
        const int shift = endian == Endian::big ? (7 - index) * 8 : index * 8;
        value |= uint64_t{p[index]} << shift;
    }
    offset_ += 8;
    return static_cast<int64_t>(value);
}

float ByteReader::f32_raw() {
    const auto bits = static_cast<uint32_t>(i32(Endian::little));
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

float ByteReader::f32_finite() {
    const float value = f32_raw();
    // NaN and infinities have all exponent bits set.
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    if ((bits & 0x7F800000u) == 0x7F800000u) {
        fail(DecodeCode::invalid_data, stage_ + "/float", offset_ - 4);
    }
    return value;
}

int32_t ByteReader::var_int() {
    uint32_t value = 0;
    for (int index = 0; index < 5; ++index) {
        const uint8_t next = u8();
        if (index == 4 && (next & 0xF0u) != 0) {
            fail(DecodeCode::invalid_data, stage_ + "/varint", offset_ - 1);
        }
        value |= uint32_t{next & 0x7Fu} << (index * 7);
        if ((next & 0x80u) == 0) {
            return static_cast<int32_t>(value);
        }
    }
    fail(DecodeCode::invalid_data, stage_ + "/varint", offset_);
}

int64_t ByteReader::var_long() {
    uint64_t result = 0;
    for (int index = 0; index < 10; ++index) {
        const uint8_t next = u8();
        if (index == 9 && (next & 0xFEu) != 0) {
            fail(DecodeCode::invalid_data, stage_ + "/varlong", offset_);
        }
        result |= uint64_t{next & 0x7Fu} << (index * 7);
        if ((next & 0x80u) == 0) {
            return static_cast<int64_t>(result);
        }
    }
    fail(DecodeCode::invalid_data, stage_ + "/varlong", offset_);
}

std::span<const uint8_t> ByteReader::bytes(uint64_t length) {
    require(length);
    auto slice = data_.subspan(offset_, length);
    offset_ += length;
    return slice;
}

void ByteReader::skip(uint64_t length) {
    require(length);
    offset_ += length;
}

std::string ByteReader::string() {
    const int32_t declared = var_int();
    const uint32_t length = budget_.length(declared, Budget::kMaxStringBytes, stage_ + "/string", offset_);
    const auto raw = bytes(length);
    std::string text(reinterpret_cast<const char *>(raw.data()), raw.size());
    if (!is_valid_utf8(text)) {
        fail(DecodeCode::invalid_data, stage_ + "/utf8", offset_);
    }
    return text;
}

bool is_valid_utf8(std::string_view text) noexcept {
    size_t index = 0;
    const size_t size = text.size();
    while (index < size) {
        const auto lead = static_cast<unsigned char>(text[index]);
        if (lead < 0x80u) {
            ++index;
            continue;
        }
        size_t extra;
        uint32_t code_point;
        if (lead >= 0xC2u && lead <= 0xDFu) {
            extra = 1;
            code_point = lead & 0x1Fu;
        } else if (lead >= 0xE0u && lead <= 0xEFu) {
            extra = 2;
            code_point = lead & 0x0Fu;
        } else if (lead >= 0xF0u && lead <= 0xF4u) {
            extra = 3;
            code_point = lead & 0x07u;
        } else {
            return false;
        }
        // The continuation bytes occupy index+1 .. index+extra and must all exist.
        if (extra >= size - index) {
            return false;
        }
        for (size_t offset = 1; offset <= extra; ++offset) {
            const auto next = static_cast<unsigned char>(text[index + offset]);
            if ((next & 0xC0u) != 0x80u) {
                return false;
            }
            code_point = (code_point << 6) | (next & 0x3Fu);
        }
        const bool overlong = (extra == 2 && code_point < 0x800u) || (extra == 3 && code_point < 0x10000u);
        const bool surrogate = code_point >= 0xD800u && code_point <= 0xDFFFu;
        if (overlong || surrogate || code_point > 0x10FFFFu) {
            return false;
        }
        index += extra + 1;
    }
    return true;
}

void secure_zero(void *data, size_t size) noexcept {
    volatile auto *bytes = static_cast<volatile unsigned char *>(data);
    for (size_t index = 0; index < size; ++index) {
        bytes[index] = 0;
    }
}

} // namespace omm::ysm
