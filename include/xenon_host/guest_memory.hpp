#ifndef XENON_HOST_GUEST_MEMORY_HPP
#define XENON_HOST_GUEST_MEMORY_HPP

#include "xenon_host/guest_module.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace xenon_host
{

class GuestMemoryLoader;
struct GuestMemoryLoadResult;

struct GuestMemoryRange
{
    GuestAddress address = 0;
    std::size_t byte_count = 0;
};

// Owns the 4 GiB virtual window required by generated Xenon code. Only the
// validated image pages are initially accessible; guest addresses are full
// 32-bit offsets from WindowBase().
class GuestMemory final
{
  public:
    static constexpr std::uint64_t WindowSize = std::uint64_t{1} << 32U;
    static constexpr std::size_t RequiredAlignment = 32U;

    ~GuestMemory();
    GuestMemory(GuestMemory&& other) noexcept;
    GuestMemory& operator=(GuestMemory&& other) noexcept;
    GuestMemory(const GuestMemory&) = delete;
    GuestMemory& operator=(const GuestMemory&) = delete;

    [[nodiscard]] std::byte* WindowBase() noexcept { return window_; }
    [[nodiscard]] const std::byte* WindowBase() const noexcept { return window_; }
    [[nodiscard]] const ImageIdentity& Identity() const noexcept { return identity_; }

    [[nodiscard]] std::span<std::byte> ImageBytes() noexcept;
    [[nodiscard]] std::span<const std::byte> ImageBytes() const noexcept;
    [[nodiscard]] std::byte* Translate(GuestMemoryRange range) noexcept;
    [[nodiscard]] const std::byte* Translate(GuestMemoryRange range) const noexcept;

  private:
    using ReleaseWindow = void (*)(std::byte* window, std::uint64_t size) noexcept;

    GuestMemory() noexcept = default;
    GuestMemory(ImageIdentity identity, std::byte* window, ReleaseWindow release) noexcept;
    void Release() noexcept;

    ImageIdentity identity_;
    std::byte* window_ = nullptr;
    ReleaseWindow release_ = nullptr;

    friend class GuestMemoryLoader;
    friend struct GuestMemoryLoadResult;
};

} // namespace xenon_host

#endif
