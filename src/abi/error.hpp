// Error reporting helpers shared by every C ABI entry point.
#pragma once

#include <cstdint>
#include <exception>
#include <new>
#include <string_view>

#include "omm/omm.h"

namespace omm::abi {

// Thrown inside the library; converted to omm_status at the C boundary.
class Failure {
public:
    Failure(omm_status code, uint32_t stage, std::string_view message,
            uint64_t source_offset = OMM_OFFSET_UNKNOWN) noexcept
        : code_(code), stage_(stage), message_(message), source_offset_(source_offset) {}

    omm_status code() const noexcept { return code_; }
    uint32_t stage() const noexcept { return stage_; }
    std::string_view message() const noexcept { return message_; }
    uint64_t source_offset() const noexcept { return source_offset_; }

private:
    omm_status code_;
    uint32_t stage_;
    // Messages are string literals or otherwise outlive the throw.
    std::string_view message_;
    uint64_t source_offset_;
};

// Clears *error when it is non-null and has a usable struct_size.
void reset_error(omm_error *error) noexcept;

// Fills *error (if usable) and returns code. Truncation never splits a UTF-8 code point.
omm_status report(omm_error *error, omm_status code, uint32_t stage, std::string_view message,
                  uint64_t source_offset = OMM_OFFSET_UNKNOWN) noexcept;

// Runs body and converts every exception into a status; body returns omm_status.
template <typename Body>
omm_status guard(omm_error *error, uint32_t stage, Body &&body) noexcept {
    reset_error(error);
    try {
        return body();
    } catch (const Failure &failure) {
        return report(error, failure.code(), failure.stage(), failure.message(),
                      failure.source_offset());
    } catch (const std::bad_alloc &) {
        return report(error, omm_error_memory, stage, "allocation failed");
    } catch (const std::exception &exception) {
        return report(error, omm_error_internal, stage, exception.what());
    } catch (...) {
        return report(error, omm_error_internal, stage, "unknown native exception");
    }
}

}  // namespace omm::abi
