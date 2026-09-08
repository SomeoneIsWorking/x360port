#include "xex_helpers.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace x360port
{
namespace
{

struct HelperPattern
{
    const std::uint8_t* bytes;
    std::size_t size;
};

} // namespace

void ScanXexHelpers(const PeImageLayout& image, std::array<std::vector<GuestAddress>, 8>& helpers)
{
    static constexpr std::array<std::uint8_t, 4> kRestGpr = {0xe9U, 0xc1U, 0xffU, 0x68U};
    static constexpr std::array<std::uint8_t, 4> kSaveGpr = {0xf9U, 0xc1U, 0xffU, 0x68U};
    static constexpr std::array<std::uint8_t, 4> kRestFpr = {0xc9U, 0xccU, 0xffU, 0x70U};
    static constexpr std::array<std::uint8_t, 4> kSaveFpr = {0xd9U, 0xccU, 0xffU, 0x70U};
    static constexpr std::array<std::uint8_t, 8> kRestVmx14 = {0x39U, 0x60U, 0xfeU, 0xe0U,
                                                               0x7dU, 0xcbU, 0x60U, 0xceU};
    static constexpr std::array<std::uint8_t, 8> kSaveVmx14 = {0x39U, 0x60U, 0xfeU, 0xe0U,
                                                               0x7dU, 0xcbU, 0x61U, 0xceU};
    static constexpr std::array<std::uint8_t, 8> kRestVmx64 = {0x39U, 0x60U, 0xfcU, 0x00U,
                                                               0x10U, 0x0bU, 0x60U, 0xcbU};
    static constexpr std::array<std::uint8_t, 8> kSaveVmx64 = {0x39U, 0x60U, 0xfcU, 0x00U,
                                                               0x10U, 0x0bU, 0x61U, 0xcbU};
    constexpr std::array<HelperPattern, 8> kPatterns = {{
        {kRestGpr.data(), kRestGpr.size()},
        {kSaveGpr.data(), kSaveGpr.size()},
        {kRestFpr.data(), kRestFpr.size()},
        {kSaveFpr.data(), kSaveFpr.size()},
        {kRestVmx14.data(), kRestVmx14.size()},
        {kSaveVmx14.data(), kSaveVmx14.size()},
        {kRestVmx64.data(), kRestVmx64.size()},
        {kSaveVmx64.data(), kSaveVmx64.size()},
    }};
    for (const PeSection& section : image.sections)
    {
        if (!section.code || section.base < image.identity.base)
        {
            continue;
        }
        const std::size_t image_offset = section.base - image.identity.base;
        if (image_offset > image.image.size() || section.size > image.image.size() - image_offset)
        {
            continue;
        }
        const std::span<const std::byte> code(image.image.data() + image_offset, section.size);
        for (std::size_t pattern_index = 0; pattern_index < kPatterns.size(); ++pattern_index)
        {
            const HelperPattern& pattern = kPatterns[pattern_index];
            for (std::size_t offset = 0; offset + pattern.size <= code.size(); offset += 4U)
            {
                bool matches = true;
                for (std::size_t byte_index = 0; byte_index < pattern.size; ++byte_index)
                {
                    if (std::to_integer<std::uint8_t>(code[offset + byte_index]) !=
                        pattern.bytes[byte_index])
                    {
                        matches = false;
                        break;
                    }
                }
                if (matches)
                {
                    helpers[pattern_index].push_back(section.base + offset);
                }
            }
        }
    }
}

} // namespace x360port
