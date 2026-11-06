// Known-answer tests for the decoder primitives that have public reference vectors.
// CityHash/ChaCha/MT use YSM-specific constants and are verified end to end by the
// OMP differential (tools/m2), not here.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "abi/error.hpp"
#include "ysmparser/crypto.hpp"
#include "ysmparser/current_container.hpp"
#include "ysmparser/json.hpp"
#include "ysmparser/resource_set.hpp"

namespace {

int failures = 0;

void expect(bool condition, const char *what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

std::vector<uint8_t> hex(const char *text) {
    std::vector<uint8_t> bytes;
    for (size_t index = 0; text[index] != '\0' && text[index + 1] != '\0'; index += 2) {
        bytes.push_back(static_cast<uint8_t>(std::stoi(std::string(text + index, 2), nullptr, 16)));
    }
    return bytes;
}

std::vector<uint8_t> text_bytes(const char *text) {
    return std::vector<uint8_t>(text, text + std::strlen(text));
}

void test_md5() {
    // RFC 1321 appendix A.5.
    expect(omm::ysm::crypto::md5(text_bytes("")) == std::array<uint8_t, 16>{0xd4, 0x1d, 0x8c, 0xd9, 0x8f, 0x00, 0xb2,
                                                                            0x04, 0xe9, 0x80, 0x09, 0x98, 0xec, 0xf8,
                                                                            0x42, 0x7e},
           "md5(\"\")");
    const auto abc = omm::ysm::crypto::md5(text_bytes("abc"));
    expect(std::vector<uint8_t>(abc.begin(), abc.end()) == hex("900150983cd24fb0d6963f7d28e17f72"), "md5(abc)");
    const auto long_text = omm::ysm::crypto::md5(
        text_bytes("12345678901234567890123456789012345678901234567890123456789012345678901234567890"));
    expect(std::vector<uint8_t>(long_text.begin(), long_text.end()) == hex("57edf4a22be3c955ac49da2e2107b67a"),
           "md5(80 digits)");
}

void test_utf8_runs() {
    using omm::ysm::json::decode_utf8_lenient;
    const std::string ascii(65537, 'a');
    expect(decode_utf8_lenient(text_bytes(ascii.c_str())) == ascii, "UTF-8 long ASCII run");
    const std::string mixed = ascii + "模型" + "\xf0\x9f\x98\x80" + ascii;
    expect(decode_utf8_lenient(text_bytes(mixed.c_str())) == mixed, "UTF-8 mixed valid runs");
    const auto bom = hex("efbbbfefbbbf41");
    expect(decode_utf8_lenient(bom) == std::string("\xef\xbb\xbf") + "A", "UTF-8 drops exactly one BOM");
    expect(decode_utf8_lenient(hex("61ff62")) == std::string("a") + "\xef\xbf\xbd" + "b",
           "UTF-8 invalid byte replacement");
    expect(decode_utf8_lenient(hex("f09f")) == "\xef\xbf\xbd", "UTF-8 truncated sequence replacement");
    expect(decode_utf8_lenient({}).empty(), "UTF-8 empty input");
    expect(decode_utf8_lenient(hex("610062")) == std::string("a\0b", 3), "UTF-8 preserves embedded NUL");
}

void test_blake3() {
    // Official BLAKE3 1.8.2 test_vectors.json; input[i] = i % 251.
    const std::pair<size_t, const char *> vectors[] = {
        {0, "af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262"},
        {1024, "42214739f095a406f3fc83deb889744ac00df831c10daa55189b5d121c855af7"},
        {1025, "d00278ae47eb27b34faecf67b4fe263f82d5412916c1ffd97c8cb7fb814b8444"},
        {8192, "aae792484c8efe4f19e2ca7d371d8c467ffb10748d8a5a1ae579948f718a2a63"},
    };
    for (const auto &[size, expected] : vectors) {
        std::vector<uint8_t> input(size);
        for (size_t index = 0; index < size; ++index) {
            input[index] = static_cast<uint8_t>(index % 251);
        }
        const auto hash = omm::ysm::crypto::blake3(input);
        expect(std::vector<uint8_t>(hash.begin(), hash.end()) == hex(expected), "BLAKE3 official vector");
    }
}

void test_current_container() {
    using namespace omm::ysm;
    auto append_integer = [](std::vector<uint8_t> &out, uint32_t value, unsigned int bytes) {
        for (unsigned int index = 0; index < bytes; ++index) {
            out.push_back(static_cast<uint8_t>(value >> (index * 8)));
        }
    };
    auto append_fixed = [](std::vector<uint8_t> &out, const std::string &text, size_t size) {
        out.insert(out.end(), text.begin(), text.end());
        out.resize(out.size() + size - text.size());
    };
    auto append_short = [](std::vector<uint8_t> &out, const std::string &text) {
        out.push_back(static_cast<uint8_t>(text.size()));
        out.insert(out.end(), text.begin(), text.end());
    };
    const std::vector<uint8_t> content{1, 2, 3};
    for (const bool compressed : {false, true}) {
        const auto stored = compressed ? hex("28b52ffd2003190000010203") : content;
        std::vector<uint8_t> table;
        auto descriptor = [&](const std::string &name, const std::string &encoding, uint32_t size,
                              uint32_t decoded_size, const std::array<uint8_t, 32> &hash) {
            append_short(table, name);
            append_short(table, encoding);
            append_integer(table, size, 4);
            append_integer(table, decoded_size, 4);
            append_integer(table, 0, 4);
            table.push_back(0);
            table.insert(table.end(), hash.begin(), hash.end());
        };
        descriptor("verification", "BLAKE3", 34, 0, {});
        descriptor("manifest", compressed ? "zstd" : "", static_cast<uint32_t>(stored.size()), compressed ? 3u : 0u,
                   crypto::blake3(content));
        auto input = hex("efbbbf59534750320a0a00");
        append_integer(input, 64, 2);
        append_integer(input, 0, 1);
        append_integer(input, 1, 2);
        append_integer(input, 0, 2);
        append_fixed(input, "unstable", 13);
        append_fixed(input, "mixel/character", 32);
        append_integer(input, 0, 2);
        append_integer(input, 2, 2);
        append_integer(input, 0, 4);
        append_integer(input, static_cast<uint32_t>(table.size()), 4);
        input.insert(input.end(), table.begin(), table.end());
        const auto header_hash = crypto::blake3(input);
        input.insert(input.end(), header_hash.begin(), header_hash.end());
        append_integer(input, 0, 2);
        input.insert(input.end(), stored.begin(), stored.end());
        Budget budget(Limits{});
        auto decoded = decode_current_container(input, budget);
        expect(decoded.chunks.find("manifest")->bytes == content, "current container raw/zstd payload");
        expect(decoded.container_id == header_hash, "current container identity");
        input.back() ^= 1;
        bool rejected = false;
        try {
            Budget corrupt_budget(Limits{});
            decode_current_container(input, corrupt_budget);
        } catch (const omm::abi::Failure &) {
            rejected = true;
        }
        expect(rejected, "current container corrupted payload rejected");
        input.back() ^= 1;
        input[30] ^= 1;
        rejected = false;
        try {
            Budget corrupt_budget(Limits{});
            decode_current_container(input, corrupt_budget);
        } catch (const omm::abi::Failure &) {
            rejected = true;
        }
        expect(rejected, "current container corrupted header rejected");
    }
}

void test_aes() {
    // NIST SP 800-38A F.2.2 / F.2.4 / F.2.6: CBC-AES128/192/256 decrypt, all four blocks.
    struct Vector {
        const char *key;
        const char *cipher;
    };

    const Vector vectors[] = {
        {"2b7e151628aed2a6abf7158809cf4f3c", "7649abac8119b246cee98e9b12e9197d5086cb9b507219ee95db113a917678b273bed6b8e"
                                             "3c1743b7116e69e222295163ff1caa1681fac09120eca307586e1a7"},
        {"8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b",
         "4f021db243bc633d7178183a9fa071e8b4d9ada9ad7dedf4e5e738763f69145a571b242012fb7ae07fa9baac3df102e008b0e27988598"
         "881d920a9e64f5615cd"},
        {"603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4",
         "f58c4c04d6e5f1ba779eabfb5f7bfbd69cfc4e967edb808d679f777bc6702c7d39f23369a9d9bacfa530e26304231461b2eb05e2c39be"
         "9fcda6c19078c6a9d1b"},
    };
    const auto iv = hex("000102030405060708090a0b0c0d0e0f");
    const auto expected_plain = hex("6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e5130c81c46a35ce411e5"
                                    "fbc1191a0a52eff69f2445df4f9b17ad2b417be66c3710");
    for (const Vector &vector : vectors) {
        std::vector<uint8_t> plain;
        const bool ok = omm::ysm::crypto::aes_cbc_decrypt(hex(vector.key), std::span<const uint8_t, 16>(iv.data(), 16),
                                                          hex(vector.cipher), plain, false);
        expect(ok && plain == expected_plain, "aes-cbc NIST vector");
        // The NIST plaintext does not end in valid PKCS#5 padding, so the default path rejects it.
        std::vector<uint8_t> unpadded;
        expect(!omm::ysm::crypto::aes_cbc_decrypt(hex(vector.key), std::span<const uint8_t, 16>(iv.data(), 16),
                                                  hex(vector.cipher), unpadded),
               "aes: invalid padding rejected");
    }
    // Wrong key and length checks.
    std::vector<uint8_t> plain;
    const auto key = hex("2b7e151628aed2a6abf7158809cf4f3c");
    expect(!omm::ysm::crypto::aes_cbc_decrypt(std::span<const uint8_t>(key.data(), 15),
                                              std::span<const uint8_t, 16>(iv.data(), 16), hex("00"), plain),
           "aes: bad key length");
    expect(!omm::ysm::crypto::aes_cbc_decrypt(key, std::span<const uint8_t, 16>(iv.data(), 16), hex("0011"), plain),
           "aes: bad cipher length");
}

void test_java_random() {
    // new java.util.Random(42).nextBytes(byte[6]) == {-0x35,... } computed from nextInt() = -1170105035, 234785527.
    uint8_t bytes[6];
    omm::ysm::crypto::java_random_next_bytes(42, bytes);
    const int32_t first = -1170105035;
    const int32_t second = 234785527;
    const uint8_t expected[6] = {
        static_cast<uint8_t>(first),       static_cast<uint8_t>(first >> 8), static_cast<uint8_t>(first >> 16),
        static_cast<uint8_t>(first >> 24), static_cast<uint8_t>(second),     static_cast<uint8_t>(second >> 8),
    };
    expect(std::memcmp(bytes, expected, 6) == 0, "java.util.Random(42).nextBytes");
}

void test_paths() {
    using omm::ysm::normalize_relative;
    expect(normalize_relative("a\\b//./c") == std::optional<std::string>("a/b/c"), "normalize separators");
    expect(normalize_relative("  ") == std::optional<std::string>(""), "blank path");
    expect(!normalize_relative("../x").has_value(), "reject ..");
    expect(!normalize_relative("/abs").has_value(), "reject absolute");
    expect(!normalize_relative("C:/x").has_value(), "reject drive");
    expect(!normalize_relative("a/b:c").has_value(), "reject colon");

    omm::ysm::ResourceSet set;
    set.add("Models/Main.json", {1}, "test");
    expect(set.exists("models/main.json"), "case-insensitive lookup");
    bool conflict = false;
    try {
        set.add("models/MAIN.json", {2}, "test");
    } catch (const omm::abi::Failure &failure) {
        conflict = failure.code() == omm_error_format;
    }
    expect(conflict, "case-only duplicate is PATH_CONFLICT");
}

} // namespace

int main() {
    test_md5();
    test_utf8_runs();
    test_blake3();
    test_current_container();
    test_aes();
    test_java_random();
    test_paths();
    if (failures != 0) {
        std::fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    std::printf("crypto_test: all checks passed\n");
    return 0;
}
