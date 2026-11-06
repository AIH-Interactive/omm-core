// Primitives required by the read-only YSGP decoders.
//
// Ported from OMP's crypto package (refs/OMP/OMP_26.3/.../avatar/ysm/crypto, MIT,
// see third_party/notices/LICENSE-Sparkle-Morpher.txt) and from the public algorithm specs:
// CityHash64 (Google), MT19937-64, java.util.Random (LCG), MD5 (RFC 1321) and
// AES (FIPS-197). Only decryption/verification paths exist; there is no encoder.
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace omm::ysm::crypto {

std::array<uint8_t, 32> blake3(std::span<const uint8_t> data) noexcept;

uint64_t city_hash64(std::span<const uint8_t> data) noexcept;
uint64_t city_hash64_with_seed(std::span<const uint8_t> data, uint64_t seed) noexcept;

// YSM's ChaCha variant: 24-byte nonce, key-mixing pre-shuffle and per-chunk rekeying.
class ChaCha {
public:
    ChaCha(std::span<const uint8_t, 32> key, std::span<const uint8_t, 24> nonce, int rounds);
    ~ChaCha();

    ChaCha(const ChaCha &) = delete;
    ChaCha &operator=(const ChaCha &) = delete;

    void process(const uint8_t *input, uint8_t *output, size_t length) noexcept;
    // Rekeys from a chunk hash and returns the next chunk size.
    uint32_t update(uint64_t hash) noexcept;

private:
    std::array<uint32_t, 16> state_{};
    std::array<uint32_t, 16> working_{};
    std::array<uint8_t, 64> stream_{};
    int rounds_;
};

class Mt19937_64 {
public:
    explicit Mt19937_64(uint64_t seed) noexcept;
    ~Mt19937_64();

    uint64_t next() noexcept;

private:
    std::array<uint64_t, 312> state_{};
    size_t index_ = 312;
};

// Bit-exact java.util.Random.nextBytes.
void java_random_next_bytes(int64_t seed, std::span<uint8_t> output) noexcept;

std::array<uint8_t, 16> md5(std::span<const uint8_t> data) noexcept;

// AES-CBC decryption with PKCS#5 unpadding; unpad=false keeps raw blocks (known-answer tests).
// Returns false on bad key/length/padding.
bool aes_cbc_decrypt(std::span<const uint8_t> key, std::span<const uint8_t, 16> iv, std::span<const uint8_t> ciphertext,
                     std::vector<uint8_t> &plaintext, bool unpad = true);

} // namespace omm::ysm::crypto
