// Gson-compatible read view over yyjson documents.
//
// OMP parses YSM JSON with lenient Gson. To keep semantics identical the view:
//  * reads numbers as raw text (Gson LazilyParsedNumber keeps the source text),
//  * resolves duplicate object keys like LinkedTreeMap (last value, first position),
//  * implements getAsString/getAsFloat/getAsInt/getAsBoolean conversions,
//  * parses floats with java.lang.Float.parseFloat rules.
// Comments, trailing commas, single quotes and unquoted keys are accepted. Gson's
// alternate '=' name/value separators remain unsupported and are reported.
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct yyjson_doc;
struct yyjson_val;
struct yyjson_mut_doc;

namespace omm::ysm::json {

// java.lang.Float.parseFloat: nullopt where Java throws NumberFormatException.
std::optional<float> parse_java_float(std::string_view text);

// java.lang.Float.toString-compatible round-trip text is not required by any consumer;
// shortest round-trip text is used wherever the reference writes then re-parses floats.
std::string float_text(float value);

// new String(bytes, UTF_8): malformed sequences become U+FFFD; a leading BOM is dropped.
std::string decode_utf8_lenient(std::span<const uint8_t> bytes);

class Value {
public:
    Value() = default;

    explicit Value(yyjson_val *value) : value_(value) {}

    bool present() const noexcept {
        return value_ != nullptr;
    }

    bool is_null() const noexcept; // JSON null (Gson JsonNull)
    bool is_object() const noexcept;
    bool is_array() const noexcept;
    bool is_primitive() const noexcept; // string, number or boolean
    bool is_string() const noexcept;
    bool is_number() const noexcept;
    bool is_bool() const noexcept;

    // Gson getAsString for primitives; Gson toString for other values.
    std::string as_string() const;
    // Gson getAsFloat / getAsInt / getAsBoolean (nullopt where Gson throws).
    std::optional<float> as_float() const;
    std::optional<int32_t> as_int() const;
    bool as_bool() const;

    // Object member lookup with Gson duplicate-key semantics; absent Value when missing.
    Value get(std::string_view key) const;

    bool has(std::string_view key) const {
        return get(key).present();
    }

    // Ordered members (deduplicated as LinkedTreeMap would hold them). Keys view the document's
    // own storage and stay valid as long as the Document lives.
    std::vector<std::pair<std::string_view, Value>> members() const;
    std::vector<Value> items() const;

    // Compact Gson JsonElement.toString().
    std::string to_json() const;

    yyjson_val *raw() const noexcept {
        return value_;
    }

private:
    yyjson_val *value_ = nullptr;
};

class Document {
public:
    Document() = default;
    ~Document();
    Document(Document &&other) noexcept;
    Document &operator=(Document &&other) noexcept;
    Document(const Document &) = delete;
    Document &operator=(const Document &) = delete;

    // Lenient parse of UTF-8 text (BOM tolerated). nullopt with error text on failure.
    static std::optional<Document> parse(std::string_view text, std::string *error = nullptr);
    // Takes ownership of an immutable copy of a mutable document (used by the binary mapper).
    static Document from_mutable(yyjson_mut_doc *document);

    Value root() const noexcept;

private:
    yyjson_doc *doc_ = nullptr;
};

// Maximum container nesting accepted by Document::parse on this thread (default 512).
// Deeply nested input is rejected before any recursive traversal can exhaust the stack.
class ScopedNestingLimit {
public:
    explicit ScopedNestingLimit(uint32_t limit);
    ~ScopedNestingLimit();
    ScopedNestingLimit(const ScopedNestingLimit &) = delete;
    ScopedNestingLimit &operator=(const ScopedNestingLimit &) = delete;

private:
    uint32_t previous_;
};

// YsmJson helpers (refs/OMP/.../avatar/ysm/YsmJson.java).
Value obj(Value parent, std::string_view key); // absent (treated as {}) when not an object
std::string string_of(Value parent, std::string_view key, const std::string &fallback);
bool bool_of(Value parent, std::string_view key, bool fallback);

} // namespace omm::ysm::json
