#include "xenon_host/guest_memory.hpp"

#include "guest_memory_load.hpp"

#include <algorithm>
#include <utility>

namespace xenon_host
{
namespace
{

[[nodiscard]] bool IsZeroDigest(const Sha256Digest& digest) noexcept
{
    return std::ranges::all_of(digest, [](std::uint8_t byte) { return byte == 0; });
}

[[nodiscard]] GuestMemoryLoadResult Refuse(GuestMemoryLoadError error,
                                           std::string_view detail) noexcept
{
    GuestMemoryLoadResult result;
    result.error = error;
    result.detail = detail;
    return result;
}

} // namespace

GuestMemory::GuestMemory(ImageIdentity identity, std::byte* window, ReleaseWindow release) noexcept
    : identity_(identity), window_(window), release_(release)
{
}

GuestMemory::~GuestMemory() { Release(); }

GuestMemory::GuestMemory(GuestMemory&& other) noexcept
    : identity_(other.identity_), window_(std::exchange(other.window_, nullptr)),
      release_(std::exchange(other.release_, nullptr))
{
}

GuestMemory& GuestMemory::operator=(GuestMemory&& other) noexcept
{
    if (this != &other)
    {
        Release();
        identity_ = other.identity_;
        window_ = std::exchange(other.window_, nullptr);
        release_ = std::exchange(other.release_, nullptr);
    }
    return *this;
}

void GuestMemory::Release() noexcept
{
    if (window_ != nullptr && release_ != nullptr)
    {
        release_(window_, WindowSize);
    }
    window_ = nullptr;
    release_ = nullptr;
}

std::span<std::byte> GuestMemory::ImageBytes() noexcept
{
    if (window_ == nullptr)
    {
        return {};
    }
    return {window_ + identity_.base, identity_.size};
}

std::span<const std::byte> GuestMemory::ImageBytes() const noexcept
{
    if (window_ == nullptr)
    {
        return {};
    }
    return {window_ + identity_.base, identity_.size};
}

std::byte* GuestMemory::Translate(GuestMemoryRange range) noexcept
{
    return const_cast<std::byte*>(std::as_const(*this).Translate(range));
}

const std::byte* GuestMemory::Translate(GuestMemoryRange range) const noexcept
{
    const std::uint64_t remaining = WindowSize - static_cast<std::uint64_t>(range.address);
    if (window_ == nullptr || range.byte_count > remaining)
    {
        return nullptr;
    }
    return window_ + range.address;
}

GuestMemoryLoadResult GuestMemoryLoader::Load(ImageIdentity identity,
                                              std::span<const std::byte> image,
                                              const GuestVirtualMemoryOps& operations) noexcept
{
    if (IsZeroDigest(identity.sha256))
    {
        return Refuse(GuestMemoryLoadError::InvalidImageDigest, "expected image SHA-256 is zero");
    }
    if (identity.size != image.size())
    {
        return Refuse(GuestMemoryLoadError::ImageSizeMismatch,
                      "image byte count does not equal the sealed image size");
    }
    const std::uint64_t image_end = static_cast<std::uint64_t>(identity.base) + identity.size;
    if (identity.size == 0 || image_end > GuestMemory::WindowSize)
    {
        return Refuse(GuestMemoryLoadError::ImageAddressOverflow,
                      "image range is empty or exceeds 32-bit space");
    }
    if (operations.reserve == nullptr || operations.commit == nullptr ||
        operations.release == nullptr)
    {
        return Refuse(GuestMemoryLoadError::ReservationFailed,
                      "guest virtual-memory operations are incomplete");
    }

    std::byte* window = operations.reserve(GuestMemory::WindowSize);
    if (window == nullptr)
    {
        return Refuse(GuestMemoryLoadError::ReservationFailed,
                      "host could not reserve the 4 GiB guest window");
    }
    GuestMemory memory(identity, window, operations.release);
    if ((reinterpret_cast<std::uintptr_t>(window) & (GuestMemory::RequiredAlignment - 1U)) != 0)
    {
        return Refuse(GuestMemoryLoadError::InvalidWindowAlignment,
                      "reserved guest window does not meet generated-code alignment");
    }
    const GuestMemoryRange image_range = {.address = identity.base, .byte_count = identity.size};
    if (!operations.commit(window, image_range))
    {
        return Refuse(GuestMemoryLoadError::CommitFailed,
                      "host could not commit the sealed guest image range");
    }

    std::ranges::copy(image, window + identity.base);
    if (HashBytes(memory.ImageBytes()) != identity.sha256)
    {
        return Refuse(GuestMemoryLoadError::ImageDigestMismatch,
                      "loaded image bytes do not match the sealed SHA-256");
    }

    GuestMemoryLoadResult result;
    result.memory = std::move(memory);
    return result;
}

} // namespace xenon_host
