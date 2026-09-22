#ifndef X360PORT_GUEST_ENDIAN_HPP
#define X360PORT_GUEST_ENDIAN_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace x360port
{

// Every scalar a guest structure holds is big-endian, so each service that
// serializes one needs the same conversion. These are that one owner: the
// helpers operate on byte spans rather than on a guest pointer, so a caller
// still passes its bytes through the bounded guest-memory contract.
inline void StoreBe16(std::span<std::byte> bytes, std::size_t offset, std::uint16_t value) noexcept
{
    bytes[offset] = static_cast<std::byte>(value >> 8U);
    bytes[offset + 1U] = static_cast<std::byte>(value);
}

inline void StoreBe32(std::span<std::byte> bytes, std::size_t offset, std::uint32_t value) noexcept
{
    bytes[offset] = static_cast<std::byte>(value >> 24U);
    bytes[offset + 1U] = static_cast<std::byte>(value >> 16U);
    bytes[offset + 2U] = static_cast<std::byte>(value >> 8U);
    bytes[offset + 3U] = static_cast<std::byte>(value);
}

[[nodiscard]] inline std::uint32_t LoadBe32(std::span<const std::byte> bytes,
                                            std::size_t offset) noexcept
{
    return (static_cast<std::uint32_t>(bytes[offset]) << 24U) |
           (static_cast<std::uint32_t>(bytes[offset + 1U]) << 16U) |
           (static_cast<std::uint32_t>(bytes[offset + 2U]) << 8U) |
           static_cast<std::uint32_t>(bytes[offset + 3U]);
}

// A guest word on its own, which is what an in/out pointer parameter carries.
using GuestWord = std::array<std::byte, 4U>;

[[nodiscard]] inline GuestWord ToGuestWord(std::uint32_t value) noexcept
{
    GuestWord bytes{};
    StoreBe32(bytes, 0U, value);
    return bytes;
}

[[nodiscard]] inline std::uint32_t FromGuestWord(const GuestWord& bytes) noexcept
{
    return LoadBe32(bytes, 0U);
}

} // namespace x360port

#endif // X360PORT_GUEST_ENDIAN_HPP
