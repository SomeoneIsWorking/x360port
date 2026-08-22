#include "guest_memory_load.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string_view>

namespace
{

using namespace xenon_host;

int checks = 0;
int failures = 0;

void Check(bool condition, std::string_view label)
{
    ++checks;
    if (!condition)
    {
        std::fprintf(stderr, "FAIL %.*s\n", static_cast<int>(label.size()), label.data());
        ++failures;
    }
}

template <std::size_t Size>
[[nodiscard]] ImageIdentity Identity(GuestAddress base, const std::array<std::byte, Size>& image)
{
    return {
        .sha256 = HashBytes(image),
        .base = base,
        .size = static_cast<std::uint32_t>(image.size()),
        .entry_point = base,
    };
}

[[nodiscard]] std::byte* RefuseReservation(std::uint64_t) noexcept { return nullptr; }

[[nodiscard]] std::byte* MisalignedReservation(std::uint64_t) noexcept
{
    return reinterpret_cast<std::byte*>(0x21U);
}

alignas(GuestMemory::RequiredAlignment) std::array<std::byte, 32> fake_window{};

[[nodiscard]] std::byte* FakeReservation(std::uint64_t) noexcept { return fake_window.data(); }

[[nodiscard]] bool RefuseCommit(std::byte*, GuestMemoryRange) noexcept { return false; }

void IgnoreRelease(std::byte*, std::uint64_t) noexcept {}

void TestExactLoadAndTranslation()
{
    std::array image = {
        std::byte{0x10}, std::byte{0x11}, std::byte{0x12}, std::byte{0x13},
        std::byte{0x14}, std::byte{0x15}, std::byte{0x16}, std::byte{0x17},
    };
    GuestMemoryLoadResult loaded = GuestMemoryLoader::Load(Identity(0x1000U, image), image);
    Check(static_cast<bool>(loaded), "exact image loads into a 4 GiB window");
    if (!loaded)
    {
        return;
    }
    GuestMemory& memory = loaded.memory;
    Check(memory.Identity().base == 0x1000U && memory.Identity().size == image.size(),
          "loaded layout matches identity");
    Check((reinterpret_cast<std::uintptr_t>(memory.WindowBase()) &
           (GuestMemory::RequiredAlignment - 1U)) == 0,
          "window base meets generated-code alignment");
    Check(memory.Translate({.address = 0x1000U, .byte_count = 4U}) == memory.ImageBytes().data(),
          "image base is the full guest offset from window base");
    Check(memory.Translate({.address = 0U, .byte_count = 1U}) == memory.WindowBase(),
          "low guest address translates from the same window base");
    Check(memory.Translate({.address = 0xffffffffU, .byte_count = 1U}) ==
              memory.WindowBase() + 0xffffffffU,
          "highest 32-bit guest byte translates inside the window");
    Check(memory.Translate({.address = 0xffffffffU, .byte_count = 2U}) == nullptr,
          "access crossing the 4 GiB window is refused");
    Check(memory.Translate({.address = 0x1000U,
                            .byte_count = std::numeric_limits<std::size_t>::max()}) == nullptr,
          "host byte-count overflow is refused");

    image[0] = std::byte{0xff};
    Check(memory.ImageBytes()[0] == std::byte{0x10}, "loaded image owns an exact snapshot");
    memory.ImageBytes()[1] = std::byte{0xaa};
    const GuestMemory& constant_memory = memory;
    const std::byte* translated = constant_memory.Translate({.address = 0x1001U, .byte_count = 1U});
    Check(translated != nullptr && *translated == std::byte{0xaa},
          "const translation observes guest writes");
}

void TestTopOfAddressSpace()
{
    constexpr std::array image = {
        std::byte{0x20},
        std::byte{0x21},
        std::byte{0x22},
        std::byte{0x23},
    };
    GuestMemoryLoadResult loaded = GuestMemoryLoader::Load(Identity(0xfffffffcU, image), image);
    Check(static_cast<bool>(loaded), "image ending exactly at 2^32 loads");
    if (loaded)
    {
        Check(loaded.memory.Translate({.address = 0xffffffffU, .byte_count = 1U}) != nullptr,
              "highest image byte translates");
    }
}

void TestLoadRefusals()
{
    constexpr std::array image = {
        std::byte{0x30},
        std::byte{0x31},
        std::byte{0x32},
        std::byte{0x33},
    };

    ImageIdentity zero_digest = Identity(0x2000U, image);
    zero_digest.sha256 = {};
    Check(GuestMemoryLoader::Load(zero_digest, image).error ==
              GuestMemoryLoadError::InvalidImageDigest,
          "zero digest is refused");

    ImageIdentity wrong_size = Identity(0x2000U, image);
    ++wrong_size.size;
    Check(GuestMemoryLoader::Load(wrong_size, image).error ==
              GuestMemoryLoadError::ImageSizeMismatch,
          "sealed byte-count mismatch is refused");

    ImageIdentity overflow = Identity(0xffffffffU, image);
    Check(GuestMemoryLoader::Load(overflow, image).error ==
              GuestMemoryLoadError::ImageAddressOverflow,
          "32-bit image range overflow is refused");

    ImageIdentity wrong_digest = Identity(0x2000U, image);
    wrong_digest.sha256[0] ^= 0xffU;
    Check(GuestMemoryLoader::Load(wrong_digest, image).error ==
              GuestMemoryLoadError::ImageDigestMismatch,
          "loaded image digest mismatch is refused");

    const GuestVirtualMemoryOps reservation_failure = {
        .reserve = RefuseReservation,
        .commit = RefuseCommit,
        .release = IgnoreRelease,
    };
    Check(GuestMemoryLoader::Load(Identity(0x2000U, image), image, reservation_failure).error ==
              GuestMemoryLoadError::ReservationFailed,
          "4 GiB reservation failure is refused");

    const GuestVirtualMemoryOps alignment_failure = {
        .reserve = MisalignedReservation,
        .commit = RefuseCommit,
        .release = IgnoreRelease,
    };
    Check(GuestMemoryLoader::Load(Identity(0x2000U, image), image, alignment_failure).error ==
              GuestMemoryLoadError::InvalidWindowAlignment,
          "misaligned guest window is refused");

    const GuestVirtualMemoryOps commit_failure = {
        .reserve = FakeReservation,
        .commit = RefuseCommit,
        .release = IgnoreRelease,
    };
    Check(GuestMemoryLoader::Load(Identity(0x2000U, image), image, commit_failure).error ==
              GuestMemoryLoadError::CommitFailed,
          "image-page commit failure is refused");
}

} // namespace

int main()
{
    TestExactLoadAndTranslation();
    TestTopOfAddressSpace();
    TestLoadRefusals();
    if (failures != 0)
    {
        std::fprintf(stderr, "%d failure(s) across %d guest-memory checks\n", failures, checks);
        return 1;
    }
    std::printf("%d guest-memory reservation/load/translation checks passed\n", checks);
    return 0;
}
