#include "ysmparser/resource_set.hpp"

#include <algorithm>
#include <map>
#include <set>

#include "miniz.h"

namespace omm::ysm {

std::optional<std::string> normalize_relative(std::string_view input) {
    std::string path(input);
    std::replace(path.begin(), path.end(), '\\', '/');

    const bool blank = std::all_of(path.begin(), path.end(), [](char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
    });
    if (blank) {
        return std::string();
    }
    const bool drive = path.size() >= 3 && ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
                       path[1] == ':' && path[2] == '/';
    if (path[0] == '/' || drive) {
        return std::nullopt;
    }

    std::string result;
    size_t start = 0;
    while (start <= path.size()) {
        size_t end = path.find('/', start);
        if (end == std::string::npos) {
            end = path.size();
        }
        const std::string_view segment(path.data() + start, end - start);
        const bool segment_blank = std::all_of(segment.begin(), segment.end(), [](char c) {
            return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
        });
        if (!segment_blank && segment != ".") {
            if (segment == ".." || segment.find(':') != std::string_view::npos ||
                segment.find('\0') != std::string_view::npos) {
                return std::nullopt;
            }
            if (!result.empty()) {
                result += '/';
            }
            result += segment;
        }
        start = end + 1;
    }
    return result;
}

std::string lower_ascii(std::string_view text) {
    std::string lower(text);
    for (char &c : lower) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return lower;
}

ResourceSet::~ResourceSet() {
    for (Resource &resource : entries_) {
        secure_clear(resource.bytes);
    }
}

void ResourceSet::add(std::string_view path, std::vector<uint8_t> bytes, std::string_view stage) {
    const auto normalized = normalize_relative(path);
    if (!normalized || normalized->empty()) {
        secure_clear(bytes);
        fail(DecodeCode::invalid_path, stage, 0);
    }
    std::string lower = lower_ascii(*normalized);
    if (lower_.count(lower) != 0) {
        secure_clear(bytes);
        fail(DecodeCode::path_conflict, stage, 0);
    }
    const size_t index = entries_.size();
    entries_.push_back(Resource{*normalized, std::move(bytes)});
    exact_.emplace(*normalized, index);
    lower_.emplace(std::move(lower), index);
}

const Resource *ResourceSet::find(std::string_view path) const {
    const auto normalized = normalize_relative(path);
    if (!normalized || normalized->empty()) {
        return nullptr;
    }
    const auto exact = exact_.find(*normalized);
    if (exact != exact_.end()) {
        return &entries_[exact->second];
    }
    const auto lower = lower_.find(lower_ascii(*normalized));
    return lower == lower_.end() ? nullptr : &entries_[lower->second];
}

bool ResourceSet::exists(std::string_view path) const {
    return find(path) != nullptr;
}

ResourceSet resources_from_entries(std::span<const EntryInput> entries, Budget &budget) {
    ResourceSet set;
    for (const EntryInput &entry : entries) {
        budget.begin_resource("entries/count", 0);
        budget.resource(entry.bytes.size(), "entries/resource", 0);
        set.add(entry.path, std::vector<uint8_t>(entry.bytes.begin(), entry.bytes.end()), "entries/path");
    }
    return set;
}

namespace {

// RAII for the miniz reader.
class ZipReader {
public:
    explicit ZipReader(std::span<const uint8_t> archive) {
        mz_zip_zero_struct(&zip_);
        open_ = mz_zip_reader_init_mem(&zip_, archive.data(), archive.size(), 0) != MZ_FALSE;
    }

    ~ZipReader() {
        if (open_) {
            mz_zip_reader_end(&zip_);
        }
    }

    ZipReader(const ZipReader &) = delete;
    ZipReader &operator=(const ZipReader &) = delete;

    bool open() const noexcept {
        return open_;
    }

    mz_zip_archive *get() noexcept {
        return &zip_;
    }

private:
    mz_zip_archive zip_;
    bool open_ = false;
};

struct ZipEntry {
    mz_uint index;
    std::string path; // normalized archive path
    uint64_t size;
};

std::string parent_directory(const std::string &path) {
    const size_t slash = path.rfind('/');
    return slash == std::string::npos ? std::string() : path.substr(0, slash);
}

size_t depth_of(const std::string &directory) {
    return directory.empty() ? 0 : static_cast<size_t>(std::count(directory.begin(), directory.end(), '/')) + 1;
}

std::string leaf_lower(const std::string &path) {
    const size_t slash = path.rfind('/');
    return lower_ascii(slash == std::string::npos ? path : path.substr(slash + 1));
}

// Extracts an entry. charge=false is used for the root-detection peek, which only bounds the
// size; the entry is charged once when it is actually kept.
std::vector<uint8_t> extract(mz_zip_archive *zip, const ZipEntry &entry, Budget &budget, bool charge = true) {
    if (charge) {
        budget.begin_resource("zip/resource-count", 0);
        budget.resource(entry.size, "zip/resource", 0);
    } else {
        budget.length(static_cast<int64_t>(entry.size), budget.limits().single_resource_bytes, "zip/peek", 0);
    }
    std::vector<uint8_t> bytes(static_cast<size_t>(entry.size));
    if (entry.size != 0 && mz_zip_reader_extract_to_mem(zip, entry.index, bytes.data(), bytes.size(), 0) == MZ_FALSE) {
        secure_clear(bytes);
        fail(DecodeCode::decompress_failed, "zip/extract", 0);
    }
    return bytes;
}

bool contains(const std::vector<uint8_t> &bytes, std::string_view needle) {
    return std::search(bytes.begin(), bytes.end(), needle.begin(), needle.end()) != bytes.end();
}

} // namespace

ResourceSet resources_from_zip(std::span<const uint8_t> archive, Budget &budget, std::string &out_root) {
    budget.length(static_cast<int64_t>(archive.size()), budget.limits().file_bytes, "zip/source", 0);
    ZipReader reader(archive);
    if (!reader.open()) {
        fail(DecodeCode::bad_header, "zip/central-directory", 0);
    }
    mz_zip_archive *zip = reader.get();

    // Index every file entry by normalized path.
    std::vector<ZipEntry> files;
    const mz_uint count = mz_zip_reader_get_num_files(zip);
    budget.length(count, budget.limits().resource_count * 4ull, "zip/entry-count", 0);
    for (mz_uint index = 0; index < count; ++index) {
        budget.check("zip/index", index);
        mz_zip_archive_file_stat stat;
        if (mz_zip_reader_file_stat(zip, index, &stat) == MZ_FALSE) {
            fail(DecodeCode::invalid_data, "zip/stat", index);
        }
        if (stat.m_is_directory) {
            continue;
        }
        if (!stat.m_is_supported) {
            fail(DecodeCode::unsupported_version, "zip/entry-method", index);
        }
        const std::string_view raw_name(stat.m_filename);
        if (!is_valid_utf8(raw_name)) {
            fail(DecodeCode::invalid_path, "zip/name-encoding", index);
        }
        const auto normalized = normalize_relative(raw_name);
        if (!normalized || normalized->empty()) {
            fail(DecodeCode::invalid_path, "zip/name", index);
        }
        files.push_back(ZipEntry{index, *normalized, stat.m_uncomp_size});
    }

    // Candidate roots: directories holding ysm.json, or a legacy main.json with geometry.
    std::map<size_t, std::set<std::string>> candidates_by_depth;
    for (const ZipEntry &entry : files) {
        const std::string leaf = leaf_lower(entry.path);
        if (leaf == "ysm.json") {
            const std::string directory = parent_directory(entry.path);
            candidates_by_depth[depth_of(directory)].insert(directory);
        }
    }
    for (const ZipEntry &entry : files) {
        if (leaf_lower(entry.path) != "main.json") {
            continue;
        }
        const std::string directory = parent_directory(entry.path);
        const size_t depth = depth_of(directory);
        auto shallower = candidates_by_depth.begin();
        if (shallower != candidates_by_depth.end() && shallower->first < depth) {
            continue; // A shallower ysm.json root already wins.
        }
        std::vector<uint8_t> bytes = extract(zip, entry, budget, false);
        const bool geometry = contains(bytes, "minecraft:geometry");
        secure_clear(bytes);
        if (geometry) {
            candidates_by_depth[depth].insert(directory);
        }
    }
    if (candidates_by_depth.empty()) {
        fail(DecodeCode::invalid_data, "zip/metadata-not-found", 0);
    }
    const std::set<std::string> &shallowest = candidates_by_depth.begin()->second;
    if (shallowest.size() != 1) {
        fail(DecodeCode::invalid_data, "zip/ambiguous-root", 0);
    }
    out_root = *shallowest.begin();

    const std::string prefix = out_root.empty() ? std::string() : out_root + "/";
    ResourceSet set;
    for (const ZipEntry &entry : files) {
        if (!prefix.empty() && entry.path.compare(0, prefix.size(), prefix) != 0) {
            continue;
        }
        set.add(entry.path.substr(prefix.size()), extract(zip, entry, budget), "zip/entry");
    }
    return set;
}

} // namespace omm::ysm
