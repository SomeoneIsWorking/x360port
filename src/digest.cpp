#include "x360port/guest_endian.hpp"
#include "x360port/module_contract.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace x360port
{
namespace
{

constexpr std::array<std::uint32_t, 64> RoundConstants = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U,
    0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU,
    0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU,
    0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
    0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
    0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U,
    0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U,
    0xc67178f2U,
};

// SHA-256 over a byte stream that holds one 64-byte block, so hashing never
// allocates and cannot fail.
class Sha256Stream
{
  public:
    void Update(std::span<const std::byte> bytes) noexcept
    {
        for (std::byte value : bytes)
        {
            block_[block_size_] = value;
            ++block_size_;
            if (block_size_ == block_.size())
            {
                Compress();
                block_size_ = 0;
            }
        }
        total_bytes_ += bytes.size();
    }

    void UpdateU32(std::uint32_t value) noexcept
    {
        std::array<std::byte, 4> bytes = {
            static_cast<std::byte>(value >> 24U), static_cast<std::byte>(value >> 16U),
            static_cast<std::byte>(value >> 8U), static_cast<std::byte>(value)};
        Update(bytes);
    }

    // Length-prefixed, so adjacent strings cannot run together.
    void UpdateString(std::string_view value) noexcept
    {
        UpdateU32(static_cast<std::uint32_t>(value.size()));
        Update(std::as_bytes(std::span(value.data(), value.size())));
    }

    [[nodiscard]] Sha256Digest Finish() noexcept
    {
        std::uint64_t bit_length = total_bytes_ * 8U;
        std::array<std::byte, 1> pad = {std::byte{0x80}};
        Update(pad);
        pad[0] = std::byte{0};
        while (block_size_ != kLengthOffset)
        {
            Update(pad);
        }
        std::array<std::byte, 8> length{};
        for (std::size_t index = 0; index < length.size(); ++index)
        {
            length[length.size() - 1U - index] =
                static_cast<std::byte>(bit_length >> static_cast<unsigned>(index * 8U));
        }
        Update(length);

        Sha256Digest digest{};
        for (std::size_t word = 0; word < state_.size(); ++word)
        {
            for (std::size_t byte = 0; byte < 4U; ++byte)
            {
                digest[word * 4U + byte] = static_cast<std::uint8_t>(
                    state_[word] >> (24U - static_cast<unsigned>(byte * 8U)));
            }
        }
        return digest;
    }

  private:
    // The message's bit length fills the last eight bytes of the final block.
    static constexpr std::size_t kLengthOffset = 56;

    void Compress() noexcept
    {
        std::array<std::uint32_t, 64> words{};
        for (std::size_t index = 0; index < 16U; ++index)
        {
            words[index] = LoadGuestWord(block_, index * 4U);
        }
        for (std::size_t index = 16; index < words.size(); ++index)
        {
            const std::uint32_t sigma0 = std::rotr(words[index - 15U], 7) ^
                                         std::rotr(words[index - 15U], 18) ^
                                         (words[index - 15U] >> 3U);
            const std::uint32_t sigma1 = std::rotr(words[index - 2U], 17) ^
                                         std::rotr(words[index - 2U], 19) ^
                                         (words[index - 2U] >> 10U);
            words[index] = words[index - 16U] + sigma0 + words[index - 7U] + sigma1;
        }

        auto working = state_;
        for (std::size_t index = 0; index < words.size(); ++index)
        {
            const std::uint32_t sum1 =
                std::rotr(working[4], 6) ^ std::rotr(working[4], 11) ^ std::rotr(working[4], 25);
            const std::uint32_t choose = (working[4] & working[5]) ^ (~working[4] & working[6]);
            const std::uint32_t temporary1 =
                working[7] + sum1 + choose + RoundConstants[index] + words[index];
            const std::uint32_t sum0 =
                std::rotr(working[0], 2) ^ std::rotr(working[0], 13) ^ std::rotr(working[0], 22);
            const std::uint32_t majority =
                (working[0] & working[1]) ^ (working[0] & working[2]) ^ (working[1] & working[2]);
            const std::uint32_t temporary2 = sum0 + majority;

            for (std::size_t lane = working.size() - 1U; lane > 0; --lane)
            {
                working[lane] = working[lane - 1U];
            }
            working[4] += temporary1;
            working[0] = temporary1 + temporary2;
        }
        for (std::size_t index = 0; index < state_.size(); ++index)
        {
            state_[index] += working[index];
        }
    }

    std::array<std::uint32_t, 8> state_ = {0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
                                           0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};
    std::array<std::byte, 64> block_{};
    std::size_t block_size_ = 0;
    std::uint64_t total_bytes_ = 0;
};

} // namespace

Sha256Digest HashBytes(std::span<const std::byte> bytes) noexcept
{
    Sha256Stream stream;
    stream.Update(bytes);
    return stream.Finish();
}

Sha256Digest HashImportManifest(std::span<const ImportRequirement> imports) noexcept
{
    Sha256Stream stream;
    for (const ImportRequirement& import : imports)
    {
        stream.UpdateU32(static_cast<std::uint32_t>(import.kind));
        stream.UpdateString(import.library);
        stream.UpdateU32(import.ordinal);
        stream.UpdateString(import.name);
        stream.UpdateU32(import.address);
        stream.UpdateU32(import.record_address);
    }
    return stream.Finish();
}

} // namespace x360port
