// Generation-checked handle registry.
//
// Handle layout (64 bits): [kind:8][generation:24][slot:32]. 0 is never issued.
// A handle is a table index, never an address; a stale generation, wrong kind or
// released slot is rejected with omm_error_handle.
#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include "abi/error.hpp"
#include "omm/omm.h"

namespace omm::abi {

enum class HandleKind : uint8_t {
    runtime = 1,
    asset = 2,
    model = 3,
    instance = 4,
};

template <typename Object>
class HandleTable {
public:
    explicit HandleTable(HandleKind kind) noexcept : kind_(kind) {}

    omm_handle insert(std::shared_ptr<Object> object) {
        std::lock_guard<std::mutex> lock(mutex_);
        uint32_t slot;
        if (!free_slots_.empty()) {
            slot = free_slots_.back();
            free_slots_.pop_back();
        } else {
            slot = static_cast<uint32_t>(entries_.size());
            entries_.push_back(Entry{});
        }
        Entry &entry = entries_[slot];
        entry.object = std::move(object);
        return encode(slot, entry.generation);
    }

    // Returns the live object or throws Failure(omm_error_handle).
    std::shared_ptr<Object> get(omm_handle handle, uint32_t stage) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return entries_[checked_slot(handle, stage)].object;
    }

    // Invalidates the handle immediately; the object dies when its last user releases it.
    std::shared_ptr<Object> remove(omm_handle handle, uint32_t stage) {
        std::lock_guard<std::mutex> lock(mutex_);
        const uint32_t slot = checked_slot(handle, stage);
        Entry &entry = entries_[slot];
        std::shared_ptr<Object> object = std::move(entry.object);
        // Generation 0 is skipped so a recycled slot never reproduces an old handle value.
        entry.generation = (entry.generation + 1u) & kGenerationMask;
        if (entry.generation == 0u) {
            entry.generation = 1u;
        }
        free_slots_.push_back(slot);
        return object;
    }

private:
    static constexpr uint32_t kGenerationMask = 0x00FFFFFFu;

    struct Entry {
        uint32_t generation = 1u;
        std::shared_ptr<Object> object;
    };

    omm_handle encode(uint32_t slot, uint32_t generation) const noexcept {
        return (static_cast<uint64_t>(kind_) << 56) |
               (static_cast<uint64_t>(generation & kGenerationMask) << 32) |
               static_cast<uint64_t>(slot);
    }

    uint32_t checked_slot(omm_handle handle, uint32_t stage) const {
        const auto kind = static_cast<uint8_t>(handle >> 56);
        const auto generation = static_cast<uint32_t>((handle >> 32) & kGenerationMask);
        const auto slot = static_cast<uint32_t>(handle & 0xFFFFFFFFu);
        if (handle == 0u || kind != static_cast<uint8_t>(kind_)) {
            throw Failure(omm_error_handle, stage, "handle is null or of the wrong kind");
        }
        if (slot >= entries_.size() || entries_[slot].generation != generation ||
            !entries_[slot].object) {
            throw Failure(omm_error_handle, stage, "handle was released or is stale");
        }
        return slot;
    }

    HandleKind kind_;
    mutable std::mutex mutex_;
    std::vector<Entry> entries_;
    std::vector<uint32_t> free_slots_;
};

}  // namespace omm::abi
