#include "ysmparser/crypto.hpp"

#include <algorithm>
#include <cstring>

#include "blake3.h"
#include "ysmparser/decode.hpp"

namespace omm::ysm::crypto {

std::array<uint8_t, 32> blake3(std::span<const uint8_t> data) noexcept {
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, data.data(), data.size());
    std::array<uint8_t, 32> hash{};
    blake3_hasher_finalize(&hasher, hash.data(), hash.size());
    return hash;
}

// ---------------------------------------------------------------------------
// CityHash64 (constants and structure identical to OMP YsmCityHash)
// ---------------------------------------------------------------------------
namespace {

constexpr uint64_t k0 = 0xE4986A230E5AAA17ull;
constexpr uint64_t k1 = 0x91AF10802CAB25A5ull;
constexpr uint64_t k2 = 0xAF29CE778879D9C7ull;
constexpr uint64_t kMul = 0xDE0F6EE09BDBAB91ull;

uint64_t fetch64(const uint8_t *p) noexcept {
    uint64_t value = 0;
    for (int index = 7; index >= 0; --index) {
        value = (value << 8) | p[index];
    }
    return value;
}

uint32_t fetch32(const uint8_t *p) noexcept {
    return uint32_t{p[0]} | (uint32_t{p[1]} << 8) | (uint32_t{p[2]} << 16) | (uint32_t{p[3]} << 24);
}

uint64_t rotate(uint64_t value, int shift) noexcept {
    return shift == 0 ? value : ((value >> shift) | (value << (64 - shift)));
}

uint64_t shift_mix(uint64_t value) noexcept {
    return value ^ (value >> 47);
}

uint64_t byte_swap64(uint64_t value) noexcept {
    uint64_t result = 0;
    for (int index = 0; index < 8; ++index) {
        result = (result << 8) | (value & 0xFFu);
        value >>= 8;
    }
    return result;
}

uint64_t hash_len16(uint64_t u, uint64_t v, uint64_t mul) noexcept {
    uint64_t a = (u ^ v) * mul;
    a ^= a >> 47;
    uint64_t b = (v ^ a) * mul;
    b ^= b >> 47;
    return b * mul;
}

uint64_t hash_len16(uint64_t u, uint64_t v) noexcept {
    return kMul * shift_mix((shift_mix((u ^ v) * kMul) ^ u) * kMul);
}

uint64_t hash_len0to16(const uint8_t *s, size_t len) noexcept {
    if (len >= 8) {
        const uint64_t mul = k2 + len * 2;
        const uint64_t a = fetch64(s) + k2;
        const uint64_t b = fetch64(s + len - 8);
        const uint64_t c = rotate(b, 37) * mul + a;
        const uint64_t d = (rotate(a, 25) + b) * mul;
        return hash_len16(c, d, mul);
    }
    if (len >= 4) {
        const uint64_t mul = k2 + len * 2;
        const uint64_t a = fetch32(s);
        return hash_len16(len + (a << 3), fetch32(s + len - 4), mul);
    }
    if (len > 0) {
        const uint32_t a = s[0];
        const uint32_t b = s[len >> 1];
        const uint32_t c = s[len - 1];
        const uint64_t y = a + (b << 8);
        const uint64_t z = len + (c << 2);
        return shift_mix(y * k2 ^ z * k0) * k2;
    }
    return k2;
}

uint64_t hash_len17to32(const uint8_t *s, size_t len) noexcept {
    const uint64_t mul = k2 + len * 2;
    const uint64_t a = fetch64(s) * k1;
    const uint64_t b = fetch64(s + 8);
    const uint64_t c = fetch64(s + len - 8) * mul;
    const uint64_t d = fetch64(s + len - 16) * k2;
    return hash_len16(rotate(a + b, 43) + rotate(c, 30) + d, a + rotate(b + k2, 18) + c, mul);
}

uint64_t hash_len33to64(const uint8_t *s, size_t len) noexcept {
    const uint64_t mul = k2 + len * 2;
    uint64_t a = fetch64(s) * k2;
    uint64_t b = fetch64(s + 8);
    const uint64_t c = fetch64(s + len - 24);
    const uint64_t d = fetch64(s + len - 32);
    const uint64_t e = fetch64(s + 16) * k2;
    const uint64_t f = fetch64(s + 24) * 9;
    const uint64_t g = fetch64(s + len - 8);
    const uint64_t h = fetch64(s + len - 16) * mul;
    const uint64_t u = rotate(a + g, 43) + (rotate(b, 30) + c) * 9;
    const uint64_t v = ((a + g) ^ d) + f + 1;
    const uint64_t w = byte_swap64((u + v) * mul) + h;
    const uint64_t x = rotate(e + f, 42) + c;
    const uint64_t y = (byte_swap64((v + w) * mul) + g) * mul;
    const uint64_t z = e + f + c;
    a = byte_swap64((x + z) * mul + y) + b;
    b = shift_mix((z + a) * mul + d + h) * mul;
    return b + x;
}

struct Pair {
    uint64_t low;
    uint64_t high;
};

Pair weak_hash_len32_with_seeds(const uint8_t *s, uint64_t seed_a, uint64_t seed_b) noexcept {
    const uint64_t w = fetch64(s);
    const uint64_t x = fetch64(s + 8);
    const uint64_t y = fetch64(s + 16);
    const uint64_t z = fetch64(s + 24);
    uint64_t a = seed_a + w;
    uint64_t b = rotate(seed_b + a + z, 21);
    const uint64_t c = a;
    a += x;
    a += y;
    b += rotate(a, 44);
    return Pair{a + z, b + c};
}

} // namespace

uint64_t city_hash64(std::span<const uint8_t> data) noexcept {
    const uint8_t *s = data.data();
    const size_t len = data.size();
    if (len <= 32) {
        return len <= 16 ? hash_len0to16(s, len) : hash_len17to32(s, len);
    }
    if (len <= 64) {
        return hash_len33to64(s, len);
    }

    uint64_t x = fetch64(s + len - 40);
    uint64_t y = fetch64(s + len - 16) + fetch64(s + len - 56);
    uint64_t z = hash_len16(fetch64(s + len - 48) + len, fetch64(s + len - 24));
    Pair v = weak_hash_len32_with_seeds(s + len - 64, len, z);
    Pair w = weak_hash_len32_with_seeds(s + len - 32, y + k1, x);
    x = x * k1 + fetch64(s);

    size_t remaining = (len - 1) & ~size_t{63};
    const uint8_t *pos = s;
    do {
        x = rotate(x + y + v.low + fetch64(pos + 8), 37) * k1;
        y = rotate(y + v.high + fetch64(pos + 48), 42) * k1;
        x ^= w.high;
        y += v.low + fetch64(pos + 40);
        z = rotate(z + w.low, 33) * k1;
        v = weak_hash_len32_with_seeds(pos, v.high * k1, x + w.low);
        w = weak_hash_len32_with_seeds(pos + 32, z + w.high, y + fetch64(pos + 16));
        std::swap(z, x);
        pos += 64;
        remaining -= 64;
    } while (remaining != 0);
    return hash_len16(hash_len16(v.low, w.low) + shift_mix(y) * k1 + z, hash_len16(v.high, w.high) + x);
}

uint64_t city_hash64_with_seed(std::span<const uint8_t> data, uint64_t seed) noexcept {
    // hash64WithSeeds(data, k2, seed) in the reference.
    return hash_len16(city_hash64(data) - k2, seed);
}

// ---------------------------------------------------------------------------
// ChaCha variant (OMP YsmChaCha)
// ---------------------------------------------------------------------------
namespace {

constexpr std::array<uint32_t, 4> kSigma = {0x61707865u, 0x3320646eu, 0x79622d32u, 0x6b206574u};

uint32_t rotl32(uint32_t value, int shift) noexcept {
    return (value << shift) | (value >> (32 - shift));
}

void quarter(std::array<uint32_t, 16> &s, int a, int b, int c, int d) noexcept {
    s[a] += s[b];
    s[d] = rotl32(s[d] ^ s[a], 16);
    s[c] += s[d];
    s[b] = rotl32(s[b] ^ s[c], 12);
    s[a] += s[b];
    s[d] = rotl32(s[d] ^ s[a], 8);
    s[c] += s[d];
    s[b] = rotl32(s[b] ^ s[c], 7);
}

void shuffle(std::array<uint32_t, 16> &s, int rounds) noexcept {
    for (int round = 0; round < rounds / 2; ++round) {
        quarter(s, 0, 4, 8, 12);
        quarter(s, 1, 5, 9, 13);
        quarter(s, 2, 6, 10, 14);
        quarter(s, 3, 7, 11, 15);
        quarter(s, 0, 5, 10, 15);
        quarter(s, 1, 6, 11, 12);
        quarter(s, 2, 7, 8, 13);
        quarter(s, 3, 4, 9, 14);
    }
}

} // namespace

ChaCha::ChaCha(std::span<const uint8_t, 32> key, std::span<const uint8_t, 24> nonce, int rounds) : rounds_(rounds) {
    for (int index = 0; index < 4; ++index) {
        working_[index] = kSigma[index];
    }
    for (int index = 0; index < 8; ++index) {
        working_[4 + index] = fetch32(key.data() + index * 4);
    }
    for (int index = 0; index < 4; ++index) {
        working_[12 + index] = fetch32(nonce.data() + index * 4);
    }
    shuffle(working_, rounds_);
    for (int index = 0; index < 4; ++index) {
        state_[index] = kSigma[index];
        state_[4 + index] = working_[index];
        state_[8 + index] = working_[12 + index];
    }
    state_[12] = 0;
    state_[13] = 0;
    state_[14] = fetch32(nonce.data() + 16);
    state_[15] = fetch32(nonce.data() + 20);
}

ChaCha::~ChaCha() {
    secure_zero(state_.data(), sizeof(state_));
    secure_zero(working_.data(), sizeof(working_));
    secure_zero(stream_.data(), sizeof(stream_));
}

void ChaCha::process(const uint8_t *input, uint8_t *output, size_t length) noexcept {
    for (size_t consumed = 0; consumed < length; consumed += 64) {
        working_ = state_;
        shuffle(working_, rounds_);
        for (int index = 0; index < 16; ++index) {
            const uint32_t value = working_[index] + state_[index];
            stream_[index * 4] = static_cast<uint8_t>(value);
            stream_[index * 4 + 1] = static_cast<uint8_t>(value >> 8);
            stream_[index * 4 + 2] = static_cast<uint8_t>(value >> 16);
            stream_[index * 4 + 3] = static_cast<uint8_t>(value >> 24);
        }
        const size_t count = std::min<size_t>(64, length - consumed);
        for (size_t index = 0; index < count; ++index) {
            output[consumed + index] = static_cast<uint8_t>(input[consumed + index] ^ stream_[index]);
        }
        if (++state_[12] == 0) {
            ++state_[13];
        }
    }
}

uint32_t ChaCha::update(uint64_t hash) noexcept {
    rounds_ = 10 * static_cast<int>(hash % 3u) + 10;
    for (int index = 4; index < 16; ++index) {
        state_[index] ^= index % 2 == 0 ? static_cast<uint32_t>(hash) : static_cast<uint32_t>(hash >> 32);
    }
    return static_cast<uint32_t>(((hash & 63u) | 64u) << 6);
}

// ---------------------------------------------------------------------------
// MT19937-64
// ---------------------------------------------------------------------------
Mt19937_64::Mt19937_64(uint64_t seed) noexcept {
    state_[0] = seed;
    for (size_t position = 1; position < state_.size(); ++position) {
        state_[position] = 6364136223846793005ull * (state_[position - 1] ^ (state_[position - 1] >> 62)) + position;
    }
}

Mt19937_64::~Mt19937_64() {
    secure_zero(state_.data(), sizeof(state_));
}

uint64_t Mt19937_64::next() noexcept {
    const size_t n = state_.size();
    if (index_ == n) {
        for (size_t position = 0; position < n; ++position) {
            const uint64_t combined =
                (state_[position] & 0xFFFFFFFF80000000ull) | (state_[(position + 1) % n] & 0x7FFFFFFFull);
            uint64_t mixed = combined >> 1;
            if ((combined & 1u) != 0) {
                mixed ^= 0xB5026F5AA96619E9ull;
            }
            state_[position] = state_[(position + 156) % n] ^ mixed;
        }
        index_ = 0;
    }
    uint64_t value = state_[index_++];
    value ^= (value >> 29) & 0x5555555555555555ull;
    value ^= (value << 17) & 0x71D67FFFEDA60000ull;
    value ^= (value << 37) & 0xFFF7EEE000000000ull;
    value ^= value >> 43;
    return value;
}

// ---------------------------------------------------------------------------
// java.util.Random
// ---------------------------------------------------------------------------
void java_random_next_bytes(int64_t seed, std::span<uint8_t> output) noexcept {
    constexpr uint64_t kMultiplier = 0x5DEECE66Dull;
    constexpr uint64_t kMask = (1ull << 48) - 1;
    uint64_t state = (static_cast<uint64_t>(seed) ^ kMultiplier) & kMask;
    size_t index = 0;
    while (index < output.size()) {
        state = (state * kMultiplier + 0xBu) & kMask;
        auto random = static_cast<int32_t>(state >> 16); // next(32)
        for (size_t n = std::min<size_t>(output.size() - index, 4); n > 0; --n) {
            output[index++] = static_cast<uint8_t>(random);
            random >>= 8;
        }
    }
}

// ---------------------------------------------------------------------------
// MD5 (RFC 1321)
// ---------------------------------------------------------------------------
std::array<uint8_t, 16> md5(std::span<const uint8_t> data) noexcept {
    static constexpr uint32_t kTable[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
        0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
        0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
        0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
        0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
        0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
        0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
    };
    static constexpr int kShift[64] = {
        7,  12, 17, 22, 7,  12, 17, 22, 7,  12, 17, 22, 7,  12, 17, 22, 5,  9,  14, 20, 5,  9,
        14, 20, 5,  9,  14, 20, 5,  9,  14, 20, 4,  11, 16, 23, 4,  11, 16, 23, 4,  11, 16, 23,
        4,  11, 16, 23, 6,  10, 15, 21, 6,  10, 15, 21, 6,  10, 15, 21, 6,  10, 15, 21,
    };

    uint32_t h0 = 0x67452301u;
    uint32_t h1 = 0xefcdab89u;
    uint32_t h2 = 0x98badcfeu;
    uint32_t h3 = 0x10325476u;

    const uint64_t bit_length = static_cast<uint64_t>(data.size()) * 8u;
    const size_t padded_size = ((data.size() + 8) / 64 + 1) * 64;
    uint8_t block[64];

    for (size_t block_start = 0; block_start < padded_size; block_start += 64) {
        // Assemble the block from message bytes, the 0x80 marker, zeros and the bit length.
        for (size_t index = 0; index < 64; ++index) {
            const size_t position = block_start + index;
            if (position < data.size()) {
                block[index] = data[position];
            } else if (position == data.size()) {
                block[index] = 0x80u;
            } else if (position >= padded_size - 8) {
                block[index] = static_cast<uint8_t>(bit_length >> ((position - (padded_size - 8)) * 8));
            } else {
                block[index] = 0;
            }
        }
        uint32_t words[16];
        for (int index = 0; index < 16; ++index) {
            words[index] = fetch32(block + index * 4);
        }
        uint32_t a = h0;
        uint32_t b = h1;
        uint32_t c = h2;
        uint32_t d = h3;
        for (int step = 0; step < 64; ++step) {
            uint32_t f;
            int g;
            if (step < 16) {
                f = (b & c) | (~b & d);
                g = step;
            } else if (step < 32) {
                f = (d & b) | (~d & c);
                g = (5 * step + 1) % 16;
            } else if (step < 48) {
                f = b ^ c ^ d;
                g = (3 * step + 5) % 16;
            } else {
                f = c ^ (b | ~d);
                g = (7 * step) % 16;
            }
            const uint32_t rotated = rotl32(a + f + kTable[step] + words[g], kShift[step]);
            a = d;
            d = c;
            c = b;
            b = b + rotated;
        }
        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
    }
    secure_zero(block, sizeof(block));

    std::array<uint8_t, 16> digest{};
    const uint32_t parts[4] = {h0, h1, h2, h3};
    for (int part = 0; part < 4; ++part) {
        for (int byte = 0; byte < 4; ++byte) {
            digest[part * 4 + byte] = static_cast<uint8_t>(parts[part] >> (byte * 8));
        }
    }
    return digest;
}

// ---------------------------------------------------------------------------
// AES decryption (FIPS-197), CBC mode, PKCS#5 padding
// ---------------------------------------------------------------------------
namespace {

constexpr uint8_t kSbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76, 0xca, 0x82, 0xc9,
    0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0, 0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f,
    0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15, 0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07,
    0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75, 0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3,
    0x29, 0xe3, 0x2f, 0x84, 0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58,
    0xcf, 0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8, 0x51, 0xa3,
    0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2, 0xcd, 0x0c, 0x13, 0xec, 0x5f,
    0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73, 0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88,
    0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb, 0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac,
    0x62, 0x91, 0x95, 0xe4, 0x79, 0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a,
    0xae, 0x08, 0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a, 0x70,
    0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e, 0xe1, 0xf8, 0x98, 0x11,
    0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf, 0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42,
    0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16,
};

struct InverseSbox {
    uint8_t table[256];

    constexpr InverseSbox() : table() {
        for (int index = 0; index < 256; ++index) {
            table[kSbox[index]] = static_cast<uint8_t>(index);
        }
    }
};

constexpr InverseSbox kInverseSbox;

constexpr uint8_t xtime(uint8_t value) noexcept {
    return static_cast<uint8_t>((value << 1) ^ ((value & 0x80u) != 0 ? 0x1Bu : 0x00u));
}

constexpr uint8_t gf_mul(uint8_t a, uint8_t b) noexcept {
    uint8_t result = 0;
    while (b != 0) {
        if ((b & 1u) != 0) {
            result ^= a;
        }
        a = xtime(a);
        b >>= 1;
    }
    return result;
}

struct InverseMixTables {
    uint8_t mul9[256]{};
    uint8_t mul11[256]{};
    uint8_t mul13[256]{};
    uint8_t mul14[256]{};

    constexpr InverseMixTables() {
        for (unsigned int value = 0; value < 256; ++value) {
            const auto byte = static_cast<uint8_t>(value);
            mul9[value] = gf_mul(byte, 9);
            mul11[value] = gf_mul(byte, 11);
            mul13[value] = gf_mul(byte, 13);
            mul14[value] = gf_mul(byte, 14);
        }
    }
};

constexpr InverseMixTables kInverseMix;

class AesDecryptor {
public:
    explicit AesDecryptor(std::span<const uint8_t> key) {
        const size_t nk = key.size() / 4;
        rounds_ = static_cast<int>(nk) + 6;
        const size_t total_words = 4 * (static_cast<size_t>(rounds_) + 1);
        uint8_t rcon = 0x01;
        for (size_t word = 0; word < total_words; ++word) {
            uint8_t *out = round_keys_ + word * 4;
            if (word < nk) {
                std::memcpy(out, key.data() + word * 4, 4);
                continue;
            }
            uint8_t temp[4];
            std::memcpy(temp, out - 4, 4);
            if (word % nk == 0) {
                const uint8_t first = temp[0];
                temp[0] = static_cast<uint8_t>(kSbox[temp[1]] ^ rcon);
                temp[1] = kSbox[temp[2]];
                temp[2] = kSbox[temp[3]];
                temp[3] = kSbox[first];
                rcon = xtime(rcon);
            } else if (nk > 6 && word % nk == 4) {
                for (uint8_t &value : temp) {
                    value = kSbox[value];
                }
            }
            for (int index = 0; index < 4; ++index) {
                out[index] = static_cast<uint8_t>(out[index - static_cast<int>(nk) * 4] ^ temp[index]);
            }
        }
    }

    ~AesDecryptor() {
        secure_zero(round_keys_, sizeof(round_keys_));
    }

    void decrypt_block(uint8_t state[16]) const noexcept {
        add_round_key(state, rounds_);
        for (int round = rounds_ - 1; round >= 0; --round) {
            inverse_shift_rows(state);
            for (int index = 0; index < 16; ++index) {
                state[index] = kInverseSbox.table[state[index]];
            }
            add_round_key(state, round);
            if (round > 0) {
                inverse_mix_columns(state);
            }
        }
    }

private:
    void add_round_key(uint8_t state[16], int round) const noexcept {
        for (int index = 0; index < 16; ++index) {
            state[index] ^= round_keys_[round * 16 + index];
        }
    }

    static void inverse_shift_rows(uint8_t s[16]) noexcept {
        // State is column-major: s[row + 4 * column].
        uint8_t temp = s[13];
        s[13] = s[9];
        s[9] = s[5];
        s[5] = s[1];
        s[1] = temp;
        std::swap(s[2], s[10]);
        std::swap(s[6], s[14]);
        temp = s[3];
        s[3] = s[7];
        s[7] = s[11];
        s[11] = s[15];
        s[15] = temp;
    }

    static void inverse_mix_columns(uint8_t s[16]) noexcept {
        for (int column = 0; column < 4; ++column) {
            uint8_t *c = s + column * 4;
            const uint8_t a0 = c[0];
            const uint8_t a1 = c[1];
            const uint8_t a2 = c[2];
            const uint8_t a3 = c[3];
            c[0] = static_cast<uint8_t>(kInverseMix.mul14[a0] ^ kInverseMix.mul11[a1] ^ kInverseMix.mul13[a2] ^
                                        kInverseMix.mul9[a3]);
            c[1] = static_cast<uint8_t>(kInverseMix.mul9[a0] ^ kInverseMix.mul14[a1] ^ kInverseMix.mul11[a2] ^
                                        kInverseMix.mul13[a3]);
            c[2] = static_cast<uint8_t>(kInverseMix.mul13[a0] ^ kInverseMix.mul9[a1] ^ kInverseMix.mul14[a2] ^
                                        kInverseMix.mul11[a3]);
            c[3] = static_cast<uint8_t>(kInverseMix.mul11[a0] ^ kInverseMix.mul13[a1] ^ kInverseMix.mul9[a2] ^
                                        kInverseMix.mul14[a3]);
        }
    }

    uint8_t round_keys_[240] = {};
    int rounds_ = 10;
};

} // namespace

bool aes_cbc_decrypt(std::span<const uint8_t> key, std::span<const uint8_t, 16> iv, std::span<const uint8_t> ciphertext,
                     std::vector<uint8_t> &plaintext, bool unpad) {
    if ((key.size() != 16 && key.size() != 24 && key.size() != 32) || ciphertext.empty() ||
        ciphertext.size() % 16 != 0) {
        return false;
    }
    AesDecryptor aes(key);
    plaintext.assign(ciphertext.begin(), ciphertext.end());
    uint8_t previous[16];
    std::memcpy(previous, iv.data(), 16);
    for (size_t offset = 0; offset < plaintext.size(); offset += 16) {
        uint8_t block_cipher[16];
        std::memcpy(block_cipher, plaintext.data() + offset, 16);
        aes.decrypt_block(plaintext.data() + offset);
        for (int index = 0; index < 16; ++index) {
            plaintext[offset + index] ^= previous[index];
        }
        std::memcpy(previous, block_cipher, 16);
    }
    if (!unpad) {
        return true;
    }
    // PKCS#5: last byte n in 1..16, and the final n bytes all equal n.
    const uint8_t pad = plaintext.back();
    bool valid = pad >= 1 && pad <= 16;
    if (valid) {
        for (size_t index = plaintext.size() - pad; index < plaintext.size(); ++index) {
            valid = valid && plaintext[index] == pad;
        }
    }
    if (!valid) {
        secure_clear(plaintext);
        return false;
    }
    plaintext.resize(plaintext.size() - pad);
    return true;
}

} // namespace omm::ysm::crypto
