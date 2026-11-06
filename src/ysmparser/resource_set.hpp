// In-memory resource set: normalized relative path -> owned bytes, insertion ordered.
//
// Path rules follow OMP YsmPackage.normalizeRelative: '\' becomes '/', absolute and
// drive paths are rejected, '.' and empty segments are dropped, '..', ':' and NUL
// are rejected. Lookup is exact first, then ASCII/Unicode-lowercase (Locale.ROOT),
// and two entries differing only by case are a PATH_CONFLICT.
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "ysmparser/decode.hpp"

namespace omm::ysm {

// Returns the normalized path, "" for blank input, or nullopt when the path is invalid.
std::optional<std::string> normalize_relative(std::string_view path);

// Lower-cases ASCII letters only; non-ASCII bytes are kept (case-insensitive lookups of
// non-ASCII names therefore require identical case, a documented narrowing of Locale.ROOT).
std::string lower_ascii(std::string_view text);

struct Resource {
    std::string path;
    std::vector<uint8_t> bytes;
};

class ResourceSet {
public:
    ResourceSet() = default;
    ~ResourceSet();
    ResourceSet(ResourceSet &&) noexcept = default;
    ResourceSet &operator=(ResourceSet &&) noexcept = default;
    ResourceSet(const ResourceSet &) = delete;
    ResourceSet &operator=(const ResourceSet &) = delete;

    // Adds an entry; throws PATH_CONFLICT on duplicates (case-insensitive) and INVALID_PATH on bad paths.
    void add(std::string_view path, std::vector<uint8_t> bytes, std::string_view stage);

    bool exists(std::string_view path) const;
    // nullptr when missing.
    const Resource *find(std::string_view path) const;

    const std::vector<Resource> &entries() const noexcept {
        return entries_;
    }

    size_t size() const noexcept {
        return entries_.size();
    }

private:
    std::vector<Resource> entries_;
    std::unordered_map<std::string, size_t> exact_;
    std::unordered_map<std::string, size_t> lower_;
};

// Raw-directory entries supplied by the host (omm_ysm_parse_entries).
struct EntryInput {
    std::string path;
    std::span<const uint8_t> bytes;
};

// Builds a resource set from host entries, enforcing the budget.
ResourceSet resources_from_entries(std::span<const EntryInput> entries, Budget &budget);

// Reads a ZIP archive. The model root is the shallowest directory containing a
// recognised metadata file (ysm.json, avatar.json, or legacy main.json); entries
// outside that root are dropped and paths become root-relative. Several candidate
// roots at the same depth are ambiguous and rejected with INVALID_DATA.
ResourceSet resources_from_zip(std::span<const uint8_t> archive, Budget &budget, std::string &out_root);

} // namespace omm::ysm
