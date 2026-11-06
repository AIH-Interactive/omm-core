// Bounded protobuf wire view for the frozen current YSM schema. Unknown fields are skipped.
#pragma once

#include <bit>
#include <cmath>
#include <optional>

#include "ysmparser/decode.hpp"

namespace omm::ysm::protobuf {

struct Field {
    uint32_t number = 0;
    uint8_t wire = 0;
    uint64_t integer = 0;
    std::span<const uint8_t> bytes;
};

inline uint64_t varint(std::span<const uint8_t> input, size_t &offset) {
    uint64_t result = 0;
    for (unsigned int shift = 0; shift < 70; shift += 7) {
        if (offset >= input.size()) {
            fail(DecodeCode::truncated, "protobuf/varint", offset);
        }
        const uint8_t value = input[offset++];
        if (shift == 63 && value > 1) {
            fail(DecodeCode::invalid_data, "protobuf/varint-overflow", offset);
        }
        result |= static_cast<uint64_t>(value & 127u) << shift;
        if ((value & 128u) == 0) {
            return result;
        }
    }
    fail(DecodeCode::invalid_data, "protobuf/varint-overflow", offset);
}

class Message {
public:
    explicit Message(std::span<const uint8_t> bytes = {}, uint32_t depth = 0) : depth_(depth) {
        if (depth > 128) {
            fail(DecodeCode::resource_limit, "protobuf/nesting", 0);
        }
        size_t offset = 0;
        while (offset < bytes.size()) {
            const uint64_t tag = varint(bytes, offset);
            Field field;
            if (tag >> 3 == 0 || tag >> 3 > 0x1FFFFFFFu) {
                fail(DecodeCode::invalid_data, "protobuf/tag", offset);
            }
            field.number = static_cast<uint32_t>(tag >> 3);
            field.wire = static_cast<uint8_t>(tag & 7u);
            if (field.wire == 0) {
                field.integer = varint(bytes, offset);
            } else {
                uint64_t length = 0;
                switch (field.wire) {
                case 1:
                    length = 8;
                    break;
                case 2:
                    length = varint(bytes, offset);
                    break;
                case 5:
                    length = 4;
                    break;
                default:
                    fail(DecodeCode::invalid_data, "protobuf/wire-type", offset);
                }
                if (length > bytes.size() - offset) {
                    fail(DecodeCode::truncated, "protobuf/field-size", offset);
                }
                field.bytes = bytes.subspan(offset, static_cast<size_t>(length));
                offset += static_cast<size_t>(length);
            }
            if (fields_.size() >= 1048576) {
                fail(DecodeCode::resource_limit, "protobuf/field-count", offset);
            }
            fields_.push_back(field);
        }
    }

    const std::vector<Field> &fields() const noexcept {
        return fields_;
    }

    const Field *last(uint32_t number) const noexcept {
        for (auto it = fields_.rbegin(); it != fields_.rend(); ++it) {
            if (it->number == number) {
                return &*it;
            }
        }
        return nullptr;
    }

    std::vector<Field> all(uint32_t number) const {
        std::vector<Field> result;
        for (const Field &field : fields_) {
            if (field.number == number) {
                result.push_back(field);
            }
        }
        return result;
    }

    static void require_wire(const Field &field, uint8_t wire) {
        if (field.wire != wire) {
            fail(DecodeCode::invalid_data, "protobuf/field-type", field.number);
        }
    }

    uint64_t integer(uint32_t number, uint64_t fallback = 0) const {
        const Field *field = last(number);
        if (field == nullptr) {
            return fallback;
        }
        require_wire(*field, 0);
        return field->integer;
    }

    std::span<const uint8_t> bytes(uint32_t number) const {
        const Field *field = last(number);
        if (field == nullptr) {
            return {};
        }
        require_wire(*field, 2);
        return field->bytes;
    }

    std::string string(uint32_t number, std::string fallback = {}) const {
        if (last(number) == nullptr) {
            return fallback;
        }
        const auto data = bytes(number);
        std::string result(reinterpret_cast<const char *>(data.data()), data.size());
        if (!is_valid_utf8(result)) {
            fail(DecodeCode::invalid_data, "protobuf/utf8", number);
        }
        return result;
    }

    Message message(uint32_t number) const {
        return Message(bytes(number), depth_ + 1);
    }

    Message message(const Field &field) const {
        require_wire(field, 2);
        return Message(field.bytes, depth_ + 1);
    }

    static float fixed_float(const Field &field) {
        require_wire(field, 5);
        uint32_t bits = 0;
        for (size_t index = 0; index < 4; ++index) {
            bits |= static_cast<uint32_t>(field.bytes[index]) << (8 * index);
        }
        const float value = std::bit_cast<float>(bits);
        if (!std::isfinite(value)) {
            fail(DecodeCode::invalid_data, "protobuf/nonfinite-float", field.number);
        }
        return value;
    }

    float number(uint32_t field, float fallback = 0) const {
        const Field *value = last(field);
        return value == nullptr ? fallback : fixed_float(*value);
    }

    std::vector<float> floats(uint32_t number) const {
        std::vector<float> result;
        for (const Field &field : all(number)) {
            if (field.wire == 5) {
                result.push_back(fixed_float(field));
                continue;
            }
            require_wire(field, 2);
            if (field.bytes.size() % 4 != 0) {
                fail(DecodeCode::invalid_data, "protobuf/packed-float", number);
            }
            for (size_t offset = 0; offset < field.bytes.size(); offset += 4) {
                result.push_back(fixed_float(Field{number, 5, 0, field.bytes.subspan(offset, 4)}));
            }
        }
        return result;
    }

    std::vector<uint64_t> integers(uint32_t number) const {
        std::vector<uint64_t> result;
        for (const Field &field : all(number)) {
            if (field.wire == 0) {
                result.push_back(field.integer);
                continue;
            }
            require_wire(field, 2);
            size_t offset = 0;
            while (offset < field.bytes.size()) {
                result.push_back(varint(field.bytes, offset));
            }
        }
        return result;
    }

private:
    uint32_t depth_;
    std::vector<Field> fields_;
};

} // namespace omm::ysm::protobuf
