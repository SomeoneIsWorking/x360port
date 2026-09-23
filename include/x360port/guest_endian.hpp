#ifndef X360PORT_GUEST_ENDIAN_HPP
#define X360PORT_GUEST_ENDIAN_HPP

#include <cstddef>
#include <cstdint>
#include <span>

namespace x360port
{

// The Xenon stores words most significant byte first. These read and write
// one 32-bit guest word at a byte offset into a copy of guest memory; the
// caller keeps the offset and the following three bytes inside the span.
[[nodiscard]] constexpr std::uint32_t LoadGuestWord(std::span<const std::byte> bytes,
                                                    std::size_t offset) noexcept
{
    return (std::to_integer<std::uint32_t>(bytes[offset]) << 24U) |
           (std::to_integer<std::uint32_t>(bytes[offset + 1U]) << 16U) |
           (std::to_integer<std::uint32_t>(bytes[offset + 2U]) << 8U) |
           std::to_integer<std::uint32_t>(bytes[offset + 3U]);
}

constexpr void StoreGuestWord(std::span<std::byte> bytes, std::size_t offset,
                              std::uint32_t value) noexcept
{
    bytes[offset] = static_cast<std::byte>(value >> 24U);
    bytes[offset + 1U] = static_cast<std::byte>(value >> 16U);
    bytes[offset + 2U] = static_cast<std::byte>(value >> 8U);
    bytes[offset + 3U] = static_cast<std::byte>(value);
}

} // namespace x360port

#endif
