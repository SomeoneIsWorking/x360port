#include "xenon_host/guest_module.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace xenon_host
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

void AppendU32(std::vector<std::byte>& bytes, std::uint32_t value)
{
    bytes.push_back(static_cast<std::byte>(value >> 24U));
    bytes.push_back(static_cast<std::byte>(value >> 16U));
    bytes.push_back(static_cast<std::byte>(value >> 8U));
    bytes.push_back(static_cast<std::byte>(value));
}

void AppendString(std::vector<std::byte>& bytes, std::string_view value)
{
    AppendU32(bytes, static_cast<std::uint32_t>(value.size()));
    for (const char character : value)
    {
        bytes.push_back(static_cast<std::byte>(character));
    }
}

[[nodiscard]] std::uint32_t LoadBigEndian(const std::byte* bytes) noexcept
{
    return (std::to_integer<std::uint32_t>(bytes[0]) << 24U) |
           (std::to_integer<std::uint32_t>(bytes[1]) << 16U) |
           (std::to_integer<std::uint32_t>(bytes[2]) << 8U) |
           std::to_integer<std::uint32_t>(bytes[3]);
}

} // namespace

Sha256Digest HashBytes(std::span<const std::byte> bytes) noexcept
{
    std::array<std::uint32_t, 8> state = {0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
                                          0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};

    const std::size_t padded_size = ((bytes.size() + 9U + 63U) / 64U) * 64U;
    std::vector<std::byte> padded(padded_size);
    for (std::size_t index = 0; index < bytes.size(); ++index)
    {
        padded[index] = bytes[index];
    }
    padded[bytes.size()] = std::byte{0x80};
    const std::uint64_t bit_length = static_cast<std::uint64_t>(bytes.size()) * 8U;
    for (std::size_t index = 0; index < 8U; ++index)
    {
        padded[padded_size - 1U - index] =
            static_cast<std::byte>(bit_length >> static_cast<unsigned>(index * 8U));
    }

    for (std::size_t block = 0; block < padded.size(); block += 64U)
    {
        std::array<std::uint32_t, 64> words{};
        for (std::size_t index = 0; index < 16U; ++index)
        {
            words[index] = LoadBigEndian(&padded[block + index * 4U]);
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

        auto working = state;
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
        for (std::size_t index = 0; index < state.size(); ++index)
        {
            state[index] += working[index];
        }
    }

    Sha256Digest digest{};
    for (std::size_t word = 0; word < state.size(); ++word)
    {
        for (std::size_t byte = 0; byte < 4U; ++byte)
        {
            digest[word * 4U + byte] =
                static_cast<std::uint8_t>(state[word] >> (24U - static_cast<unsigned>(byte * 8U)));
        }
    }
    return digest;
}

Sha256Digest HashFunctionMap(std::span<const FunctionMapping> mappings) noexcept
{
    std::vector<std::byte> canonical;
    canonical.reserve(mappings.size() * sizeof(GuestAddress));
    for (const FunctionMapping& mapping : mappings)
    {
        AppendU32(canonical, mapping.address);
    }
    return HashBytes(canonical);
}

Sha256Digest HashImportManifest(std::span<const ImportRequirement> imports) noexcept
{
    std::vector<std::byte> canonical;
    for (const ImportRequirement& import : imports)
    {
        AppendString(canonical, import.library);
        AppendU32(canonical, import.ordinal);
        AppendString(canonical, import.name);
    }
    return HashBytes(canonical);
}

} // namespace xenon_host
