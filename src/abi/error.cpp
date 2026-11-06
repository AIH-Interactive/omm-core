#include "abi/error.hpp"

#include <cstring>

namespace omm::abi {

namespace {

// An omm_error is writable only when the caller declared at least the full current layout.
bool usable(const omm_error *error) noexcept {
    return error != nullptr && error->struct_size >= sizeof(omm_error);
}

// Largest prefix length <= limit that does not end inside a UTF-8 multi-byte sequence.
size_t utf8_safe_length(std::string_view text, size_t limit) noexcept {
    if (text.size() <= limit) {
        return text.size();
    }
    size_t length = limit;
    // Step back over continuation bytes (10xxxxxx) so the cut lands on a code point start.
    while (length > 0 && (static_cast<unsigned char>(text[length]) & 0xC0u) == 0x80u) {
        --length;
    }
    return length;
}

}  // namespace

void reset_error(omm_error *error) noexcept {
    if (!usable(error)) {
        return;
    }
    const uint32_t declared_size = error->struct_size;
    std::memset(error, 0, sizeof(omm_error));
    error->struct_size = declared_size;
    error->code = omm_ok;
    error->stage = omm_stage_none;
    error->source_offset = OMM_OFFSET_UNKNOWN;
}

omm_status report(omm_error *error, omm_status code, uint32_t stage, std::string_view message,
                  uint64_t source_offset) noexcept {
    if (!usable(error)) {
        return code;
    }
    error->code = code;
    error->stage = stage;
    error->source_offset = source_offset;
    const size_t length = utf8_safe_length(message, sizeof(error->message) - 1);
    std::memcpy(error->message, message.data(), length);
    error->message[length] = '\0';
    return code;
}

}  // namespace omm::abi
