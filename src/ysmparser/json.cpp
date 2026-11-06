#include "ysmparser/json.hpp"

#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <unordered_map>

#include "yyjson.h"

namespace omm::ysm::json {

namespace {

bool is_java_trim_char(char c) noexcept {
    return static_cast<unsigned char>(c) <= ' ';
}

std::string_view java_trim(std::string_view text) noexcept {
    size_t start = 0;
    size_t end = text.size();
    while (start < end && is_java_trim_char(text[start])) {
        ++start;
    }
    while (end > start && is_java_trim_char(text[end - 1])) {
        --end;
    }
    return text.substr(start, end - start);
}

bool is_digit(char c) noexcept {
    return c >= '0' && c <= '9';
}

void append_utf8(std::string &out, uint32_t code_point) {
    if (code_point < 0x80) {
        out += static_cast<char>(code_point);
    } else if (code_point < 0x800) {
        out += static_cast<char>(0xC0 | (code_point >> 6));
        out += static_cast<char>(0x80 | (code_point & 0x3F));
    } else if (code_point < 0x10000) {
        out += static_cast<char>(0xE0 | (code_point >> 12));
        out += static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (code_point & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (code_point >> 18));
        out += static_cast<char>(0x80 | ((code_point >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (code_point & 0x3F));
    }
}

// Decodes one UTF-8 code point at text[index]. Returns the sequence length, or the length
// of the maximal invalid subpart (>= 1) with *valid = false.
size_t decode_one(std::string_view text, size_t index, uint32_t &code_point, bool &valid) {
    const auto lead = static_cast<unsigned char>(text[index]);
    valid = false;
    if (lead < 0x80) {
        code_point = lead;
        valid = true;
        return 1;
    }
    size_t extra;
    unsigned char low = 0x80;
    unsigned char high = 0xBF;
    if (lead >= 0xC2 && lead <= 0xDF) {
        extra = 1;
        code_point = lead & 0x1F;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
        extra = 2;
        code_point = lead & 0x0F;
        if (lead == 0xE0)
            low = 0xA0;
        if (lead == 0xED)
            high = 0x9F;
    } else if (lead >= 0xF0 && lead <= 0xF4) {
        extra = 3;
        code_point = lead & 0x07;
        if (lead == 0xF0)
            low = 0x90;
        if (lead == 0xF4)
            high = 0x8F;
    } else {
        return 1;
    }
    for (size_t offset = 1; offset <= extra; ++offset) {
        if (index + offset >= text.size()) {
            return offset;
        }
        const auto next = static_cast<unsigned char>(text[index + offset]);
        const unsigned char min = offset == 1 ? low : 0x80;
        const unsigned char max = offset == 1 ? high : 0xBF;
        if (next < min || next > max) {
            return offset;
        }
        code_point = (code_point << 6) | (next & 0x3F);
    }
    valid = true;
    return extra + 1;
}

// Gson JsonWriter string escaping (htmlSafe = false).
void append_json_string(std::string &out, std::string_view text) {
    out += '"';
    size_t index = 0;
    while (index < text.size()) {
        uint32_t code_point;
        bool valid;
        const size_t length = decode_one(text, index, code_point, valid);
        if (!valid) {
            code_point = 0xFFFD;
        }
        switch (code_point) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\t':
            out += "\\t";
            break;
        case '\b':
            out += "\\b";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\f':
            out += "\\f";
            break;
        case 0x2028:
            out += "\\u2028";
            break;
        case 0x2029:
            out += "\\u2029";
            break;
        default:
            if (code_point < 0x20) {
                char buffer[8];
                std::snprintf(buffer, sizeof(buffer), "\\u%04x", code_point);
                out += buffer;
            } else {
                append_utf8(out, code_point);
            }
        }
        index += length;
    }
    out += '"';
}

void append_value(std::string &out, Value value) {
    if (!value.present() || value.is_null()) {
        out += "null";
    } else if (value.is_object()) {
        out += '{';
        bool first = true;
        for (const auto &member : value.members()) {
            if (!first) {
                out += ',';
            }
            first = false;
            append_json_string(out, member.first);
            out += ':';
            append_value(out, member.second);
        }
        out += '}';
    } else if (value.is_array()) {
        out += '[';
        bool first = true;
        for (const Value &item : value.items()) {
            if (!first) {
                out += ',';
            }
            first = false;
            append_value(out, item);
        }
        out += ']';
    } else if (value.is_string()) {
        append_json_string(out, value.as_string());
    } else {
        out += value.as_string();
    }
}

thread_local uint32_t g_nesting_limit = 512;

// Iterative depth check (no recursion on attacker-controlled depth).
bool within_nesting_limit(yyjson_val *root) {
    std::vector<std::pair<yyjson_val *, uint32_t>> stack;
    if (root != nullptr) {
        stack.emplace_back(root, 1);
    }
    while (!stack.empty()) {
        const auto [value, depth] = stack.back();
        stack.pop_back();
        if (!yyjson_is_ctn(value)) {
            continue;
        }
        if (depth > g_nesting_limit) {
            return false;
        }
        if (yyjson_is_arr(value)) {
            yyjson_arr_iter iter;
            yyjson_arr_iter_init(value, &iter);
            while (yyjson_val *item = yyjson_arr_iter_next(&iter)) {
                stack.emplace_back(item, depth + 1);
            }
        } else {
            yyjson_obj_iter iter;
            yyjson_obj_iter_init(value, &iter);
            while (yyjson_val *key = yyjson_obj_iter_next(&iter)) {
                stack.emplace_back(yyjson_obj_iter_get_val(key), depth + 1);
            }
        }
    }
    return true;
}

} // namespace

std::optional<float> parse_java_float(std::string_view input) {
    std::string_view text = java_trim(input);
    if (text.empty()) {
        return std::nullopt;
    }
    bool negative = false;
    size_t index = 0;
    if (text[0] == '+' || text[0] == '-') {
        negative = text[0] == '-';
        index = 1;
    }
    const std::string_view body = text.substr(index);
    if (body == "NaN") {
        return std::numeric_limits<float>::quiet_NaN();
    }
    if (body == "Infinity") {
        return negative ? -std::numeric_limits<float>::infinity() : std::numeric_limits<float>::infinity();
    }

    std::string_view digits = body;
    if (!digits.empty()) {
        const char last = digits.back();
        if (last == 'f' || last == 'F' || last == 'd' || last == 'D') {
            digits.remove_suffix(1);
        }
    }

    float value = 0.0f;
    if (digits.size() > 2 && digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X')) {
        // Hex float: 0x<hex>[.<hex>]p[+-]<dec> (binary exponent is mandatory in Java).
        const std::string_view hex = digits.substr(2);
        const size_t p = hex.find_first_of("pP");
        if (p == std::string_view::npos || p == 0) {
            return std::nullopt;
        }
        const auto result = std::from_chars(hex.data(), hex.data() + hex.size(), value, std::chars_format::hex);
        if (result.ec != std::errc() && result.ec != std::errc::result_out_of_range) {
            return std::nullopt;
        }
        if (result.ptr != hex.data() + hex.size()) {
            return std::nullopt;
        }
    } else {
        // Decimal grammar: digits [. digits] | . digits, optional exponent.
        size_t position = 0;
        size_t mantissa_digits = 0;
        while (position < digits.size() && is_digit(digits[position])) {
            ++position;
            ++mantissa_digits;
        }
        if (position < digits.size() && digits[position] == '.') {
            ++position;
            while (position < digits.size() && is_digit(digits[position])) {
                ++position;
                ++mantissa_digits;
            }
        }
        if (mantissa_digits == 0) {
            return std::nullopt;
        }
        if (position < digits.size() && (digits[position] == 'e' || digits[position] == 'E')) {
            ++position;
            if (position < digits.size() && (digits[position] == '+' || digits[position] == '-')) {
                ++position;
            }
            size_t exponent_digits = 0;
            while (position < digits.size() && is_digit(digits[position])) {
                ++position;
                ++exponent_digits;
            }
            if (exponent_digits == 0) {
                return std::nullopt;
            }
        }
        if (position != digits.size()) {
            return std::nullopt;
        }
        const auto result = std::from_chars(digits.data(), digits.data() + digits.size(), value);
        if (result.ec == std::errc::result_out_of_range) {
            // Overflow → infinity, underflow → zero, as Java does.
            double wide = 0.0;
            std::from_chars(digits.data(), digits.data() + digits.size(), wide);
            value = std::fabs(wide) >= 1.0 ? std::numeric_limits<float>::infinity() : 0.0f;
        } else if (result.ec != std::errc()) {
            return std::nullopt;
        }
    }
    return negative ? -value : value;
}

std::string float_text(float value) {
    char buffer[64];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    return std::string(buffer, result.ptr);
}

std::string decode_utf8_lenient(std::span<const uint8_t> bytes) {
    std::string_view text(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    std::string out;
    out.reserve(text.size());
    size_t index = 0;
    size_t valid_start = 0;
    while (index < text.size()) {
        // Model JSON is predominantly ASCII. Scan whole machine words, then copy each
        // valid run once instead of making a string append call for every code point.
        if (text.size() - index >= sizeof(uint64_t)) {
            uint64_t word;
            std::memcpy(&word, text.data() + index, sizeof(word));
            if ((word & 0x8080808080808080ull) == 0) {
                index += sizeof(word);
                continue;
            }
        }
        if (static_cast<unsigned char>(text[index]) < 0x80) {
            ++index;
            continue;
        }
        uint32_t code_point;
        bool valid;
        const size_t length = decode_one(text, index, code_point, valid);
        if (!valid) {
            out.append(text.data() + valid_start, index - valid_start);
            append_utf8(out, 0xFFFD);
            index += length;
            valid_start = index;
            continue;
        }
        index += length;
    }
    if (valid_start < text.size()) {
        out.append(text.data() + valid_start, text.size() - valid_start);
    }
    // readString drops one leading U+FEFF.
    if (out.size() >= 3 && static_cast<unsigned char>(out[0]) == 0xEF && static_cast<unsigned char>(out[1]) == 0xBB &&
        static_cast<unsigned char>(out[2]) == 0xBF) {
        out.erase(0, 3);
    }
    return out;
}

// ---- Value ----

bool Value::is_null() const noexcept {
    return value_ != nullptr && yyjson_is_null(value_);
}

bool Value::is_object() const noexcept {
    return value_ != nullptr && yyjson_is_obj(value_);
}

bool Value::is_array() const noexcept {
    return value_ != nullptr && yyjson_is_arr(value_);
}

bool Value::is_string() const noexcept {
    return value_ != nullptr && yyjson_is_str(value_);
}

bool Value::is_number() const noexcept {
    return value_ != nullptr && (yyjson_is_raw(value_) || yyjson_is_num(value_));
}

bool Value::is_bool() const noexcept {
    return value_ != nullptr && yyjson_is_bool(value_);
}

bool Value::is_primitive() const noexcept {
    return is_string() || is_number() || is_bool();
}

std::string Value::as_string() const {
    if (value_ == nullptr) {
        return std::string();
    }
    if (yyjson_is_str(value_)) {
        return std::string(yyjson_get_str(value_), yyjson_get_len(value_));
    }
    if (yyjson_is_raw(value_)) {
        return std::string(yyjson_get_raw(value_), yyjson_get_len(value_));
    }
    if (yyjson_is_bool(value_)) {
        return yyjson_get_bool(value_) ? "true" : "false";
    }
    if (yyjson_is_num(value_)) {
        // Only reachable for documents built without NUMBER_AS_RAW.
        return float_text(static_cast<float>(yyjson_get_num(value_)));
    }
    return to_json();
}

std::optional<float> Value::as_float() const {
    // JsonPrimitive.getAsFloat: Float.parseFloat of the text; booleans and containers fail.
    if (!is_primitive() || is_bool()) {
        return std::nullopt;
    }
    return parse_java_float(as_string());
}

std::optional<int32_t> Value::as_int() const {
    if (!is_primitive()) {
        return std::nullopt;
    }
    const std::string text = as_string();
    const bool number = is_number();
    // Integer.parseInt (no whitespace, optional sign).
    int64_t wide = 0;
    const char *begin = text.data();
    const char *end = text.data() + text.size();
    const char *digits = begin;
    if (digits != end && *digits == '+') {
        ++digits;
    }
    auto parsed = std::from_chars(digits, end, wide);
    if (parsed.ec == std::errc() && parsed.ptr == end && wide >= std::numeric_limits<int32_t>::min() &&
        wide <= std::numeric_limits<int32_t>::max()) {
        return static_cast<int32_t>(wide);
    }
    if (!number) {
        return std::nullopt;
    }
    // LazilyParsedNumber.intValue: (int) Long.parseLong, then BigDecimal(value).intValue().
    if (parsed.ec == std::errc() && parsed.ptr == end) {
        return static_cast<int32_t>(static_cast<uint32_t>(static_cast<uint64_t>(wide)));
    }
    double decimal = 0.0;
    const auto decimal_result = std::from_chars(begin, end, decimal);
    if (decimal_result.ec != std::errc() || decimal_result.ptr != end || !std::isfinite(decimal)) {
        return std::nullopt;
    }
    const double truncated = std::trunc(decimal);
    if (std::fabs(truncated) < 9.2e18) {
        return static_cast<int32_t>(static_cast<uint32_t>(static_cast<uint64_t>(static_cast<int64_t>(truncated))));
    }
    return std::nullopt;
}

bool Value::as_bool() const {
    if (is_bool()) {
        return yyjson_get_bool(value_);
    }
    const std::string text = as_string();
    if (text.size() != 4) {
        return false;
    }
    std::string lower = text;
    for (char &c : lower) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return lower == "true";
}

Value Value::get(std::string_view key) const {
    if (!is_object()) {
        return Value();
    }
    yyjson_val *found = nullptr;
    yyjson_obj_iter iter;
    yyjson_obj_iter_init(value_, &iter);
    yyjson_val *name;
    while ((name = yyjson_obj_iter_next(&iter)) != nullptr) {
        if (yyjson_get_len(name) == key.size() && std::memcmp(yyjson_get_str(name), key.data(), key.size()) == 0) {
            found = yyjson_obj_iter_get_val(name); // keep the last duplicate
        }
    }
    return Value(found);
}

std::vector<std::pair<std::string_view, Value>> Value::members() const {
    std::vector<std::pair<std::string_view, Value>> result;
    if (!is_object()) {
        return result;
    }
    result.reserve(yyjson_obj_size(value_));
    // Duplicate keys are rare; a hash index is only built for large objects.
    constexpr size_t kLinearLimit = 16;
    const bool use_index = yyjson_obj_size(value_) > kLinearLimit;
    std::unordered_map<std::string_view, size_t> positions;
    yyjson_obj_iter iter;
    yyjson_obj_iter_init(value_, &iter);
    yyjson_val *name;
    while ((name = yyjson_obj_iter_next(&iter)) != nullptr) {
        const std::string_view key(yyjson_get_str(name), yyjson_get_len(name));
        const Value member(yyjson_obj_iter_get_val(name));
        size_t existing = result.size();
        if (use_index) {
            const auto found = positions.find(key);
            if (found != positions.end()) {
                existing = found->second;
            } else {
                positions.emplace(key, result.size());
            }
        } else {
            for (size_t index = 0; index < result.size(); ++index) {
                if (result[index].first == key) {
                    existing = index;
                    break;
                }
            }
        }
        if (existing != result.size()) {
            result[existing].second = member;
        } else {
            result.emplace_back(key, member);
        }
    }
    return result;
}

std::vector<Value> Value::items() const {
    std::vector<Value> result;
    if (!is_array()) {
        return result;
    }
    yyjson_arr_iter iter;
    yyjson_arr_iter_init(value_, &iter);
    yyjson_val *item;
    while ((item = yyjson_arr_iter_next(&iter)) != nullptr) {
        result.emplace_back(item);
    }
    return result;
}

std::string Value::to_json() const {
    std::string out;
    append_value(out, *this);
    return out;
}

// ---- Document ----

Document::~Document() {
    if (doc_ != nullptr) {
        yyjson_doc_free(doc_);
    }
}

Document::Document(Document &&other) noexcept : doc_(other.doc_) {
    other.doc_ = nullptr;
}

Document &Document::operator=(Document &&other) noexcept {
    if (this != &other) {
        if (doc_ != nullptr) {
            yyjson_doc_free(doc_);
        }
        doc_ = other.doc_;
        other.doc_ = nullptr;
    }
    return *this;
}

std::optional<Document> Document::parse(std::string_view text, std::string *error) {
    const yyjson_read_flag flags =
        YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS | YYJSON_READ_ALLOW_INF_AND_NAN |
        YYJSON_READ_NUMBER_AS_RAW | YYJSON_READ_ALLOW_INVALID_UNICODE | YYJSON_READ_ALLOW_BOM |
        YYJSON_READ_ALLOW_SINGLE_QUOTED_STR | YYJSON_READ_ALLOW_UNQUOTED_KEY | YYJSON_READ_ALLOW_EXT_ESCAPE;
    yyjson_read_err read_error;
    yyjson_doc *doc = yyjson_read_opts(const_cast<char *>(text.data()), text.size(), flags, nullptr, &read_error);
    if (doc == nullptr) {
        if (error != nullptr) {
            *error = std::string(read_error.msg) + " at byte " + std::to_string(read_error.pos);
        }
        return std::nullopt;
    }
    Document document;
    document.doc_ = doc;
    if (!within_nesting_limit(yyjson_doc_get_root(doc))) {
        if (error != nullptr) {
            *error = "nesting deeper than " + std::to_string(g_nesting_limit);
        }
        return std::nullopt;
    }
    return document;
}

ScopedNestingLimit::ScopedNestingLimit(uint32_t limit) : previous_(g_nesting_limit) {
    if (limit != 0) {
        g_nesting_limit = limit;
    }
}

ScopedNestingLimit::~ScopedNestingLimit() {
    g_nesting_limit = previous_;
}

Document Document::from_mutable(yyjson_mut_doc *document) {
    Document result;
    result.doc_ = yyjson_mut_doc_imut_copy(document, nullptr);
    if (result.doc_ == nullptr) {
        throw std::bad_alloc();
    }
    return result;
}

Value Document::root() const noexcept {
    return Value(doc_ == nullptr ? nullptr : yyjson_doc_get_root(doc_));
}

// ---- YsmJson helpers ----

Value obj(Value parent, std::string_view key) {
    const Value element = parent.get(key);
    return element.is_object() ? element : Value();
}

std::string string_of(Value parent, std::string_view key, const std::string &fallback) {
    const Value element = parent.get(key);
    return element.is_primitive() ? element.as_string() : fallback;
}

bool bool_of(Value parent, std::string_view key, bool fallback) {
    const Value element = parent.get(key);
    return element.is_bool() ? element.as_bool() : fallback;
}

} // namespace omm::ysm::json
