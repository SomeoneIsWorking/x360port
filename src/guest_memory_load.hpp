#ifndef XENON_HOST_GUEST_MEMORY_LOAD_HPP
#define XENON_HOST_GUEST_MEMORY_LOAD_HPP

#include "xenon_host/guest_memory.hpp"

#include <string_view>

namespace xenon_host
{

enum class GuestMemoryLoadError : std::uint8_t
{
    None,
    InvalidImageDigest,
    ImageSizeMismatch,
    ImageAddressOverflow,
    ReservationFailed,
    InvalidWindowAlignment,
    CommitFailed,
    ImageDigestMismatch,
};

struct GuestVirtualMemoryOps
{
    std::byte* (*reserve)(std::uint64_t size) noexcept = nullptr;
    bool (*commit)(std::byte* window, GuestMemoryRange range) noexcept = nullptr;
    void (*release)(std::byte* window, std::uint64_t size) noexcept = nullptr;
};

struct GuestMemoryLoadResult
{
    GuestMemory memory;
    GuestMemoryLoadError error = GuestMemoryLoadError::None;
    std::string_view detail;

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return error == GuestMemoryLoadError::None && memory.WindowBase() != nullptr;
    }
};

class GuestMemoryLoader final
{
  public:
    [[nodiscard]] static GuestMemoryLoadResult
    Load(ImageIdentity identity, std::span<const std::byte> image,
         const GuestVirtualMemoryOps& operations = PlatformOperations()) noexcept;

    [[nodiscard]] static const GuestVirtualMemoryOps& PlatformOperations() noexcept;
};

} // namespace xenon_host

#endif
