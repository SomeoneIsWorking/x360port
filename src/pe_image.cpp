#include "x360port/pe_image.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace x360port
{
namespace
{

constexpr std::uint16_t kPowerPcBeMachine = 0x01F2U;
constexpr std::uint16_t kPe32Magic = 0x010BU;
constexpr std::uint32_t kCodeSection = 0x00000020U;

[[nodiscard]] PeImageLayoutResult Refuse(std::string message)
{
    return {.layout = {}, .error = std::move(message)};
}

[[nodiscard]] bool HasBytes(std::span<const std::byte> bytes, std::size_t offset,
                            std::size_t count) noexcept
{
    return offset <= bytes.size() && count <= bytes.size() - offset;
}

[[nodiscard]] std::uint16_t ReadU16(std::span<const std::byte> bytes, std::size_t offset) noexcept
{
    return static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset])) |
           static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset + 1U]) << 8U);
}

[[nodiscard]] std::uint32_t ReadU32(std::span<const std::byte> bytes, std::size_t offset) noexcept
{
    return static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset])) |
           (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 1U])) << 8U) |
           (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 2U])) << 16U) |
           (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 3U])) << 24U);
}

[[nodiscard]] std::string SectionName(std::span<const std::byte> bytes, std::size_t offset)
{
    std::string name;
    for (std::size_t index = 0; index < 8U; ++index)
    {
        const char character =
            static_cast<char>(std::to_integer<std::uint8_t>(bytes[offset + index]));
        if (character == '\0')
        {
            break;
        }
        name.push_back(character);
    }
    return name;
}

} // namespace

PeImageLayoutResult MapPeImage(std::span<const std::byte> source)
{
    if (!HasBytes(source, 0, 0x40U) || std::to_integer<std::uint8_t>(source[0]) != 'M' ||
        std::to_integer<std::uint8_t>(source[1]) != 'Z')
    {
        return Refuse("PE image is missing its DOS header");
    }

    const std::uint32_t pe_offset = ReadU32(source, 0x3CU);
    if (!HasBytes(source, pe_offset, 4U + 20U) || ReadU32(source, pe_offset) != 0x00004550U)
    {
        return Refuse("PE image has an invalid NT header offset or signature");
    }

    const std::size_t file_header = static_cast<std::size_t>(pe_offset) + 4U;
    const std::uint16_t machine = ReadU16(source, file_header);
    const std::uint16_t section_count = ReadU16(source, file_header + 2U);
    const std::uint16_t optional_size = ReadU16(source, file_header + 16U);
    if (machine != kPowerPcBeMachine || section_count == 0U || section_count > 96U ||
        optional_size < 224U)
    {
        return Refuse("PE image is not a supported Xbox 360 PE32 image");
    }

    const std::size_t optional = file_header + 20U;
    if (!HasBytes(source, optional, optional_size) || ReadU16(source, optional) != kPe32Magic)
    {
        return Refuse("PE image has an invalid PE32 optional header");
    }

    const GuestAddress image_base = ReadU32(source, optional + 28U);
    const std::uint32_t entry_rva = ReadU32(source, optional + 16U);
    const std::uint32_t image_size = ReadU32(source, optional + 56U);
    const std::uint32_t header_size = ReadU32(source, optional + 60U);
    const std::uint32_t section_alignment = ReadU32(source, optional + 32U);
    if (image_base == 0U || (image_base & (kGuestImageBaseAlignment - 1U)) != 0U ||
        image_size == 0U || header_size == 0U || header_size > image_size ||
        section_alignment == 0U || !HasBytes(source, 0, header_size))
    {
        return Refuse("PE image has invalid guest-image geometry");
    }

    const std::size_t section_table = optional + optional_size;
    const std::size_t section_bytes = static_cast<std::size_t>(section_count) * 40U;
    if (!HasBytes(source, section_table, section_bytes))
    {
        return Refuse("PE section table exceeds the normalized image");
    }

    PeImageLayout layout;
    layout.source_sha256 = HashBytes(source);
    layout.image.assign(image_size, std::byte{0});
    std::copy_n(source.begin(), header_size, layout.image.begin());
    layout.identity.base = image_base;
    layout.identity.size = image_size;

    std::uint64_t code_begin = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t code_end = 0;
    for (std::size_t index = 0; index < section_count; ++index)
    {
        const std::size_t section = section_table + index * 40U;
        const std::uint32_t virtual_size = ReadU32(source, section + 8U);
        const std::uint32_t virtual_address = ReadU32(source, section + 12U);
        const std::uint32_t raw_size = ReadU32(source, section + 16U);
        const std::uint32_t raw_offset = ReadU32(source, section + 20U);
        const std::uint32_t characteristics = ReadU32(source, section + 36U);
        const std::uint32_t mapped_size = std::max(virtual_size, raw_size);
        const std::uint64_t mapped_end = static_cast<std::uint64_t>(virtual_address) + mapped_size;
        if (mapped_end > image_size || (raw_size != 0U && !HasBytes(source, raw_offset, raw_size)))
        {
            return Refuse("PE section exceeds the normalized image or source bytes");
        }

        const bool code = (characteristics & kCodeSection) != 0U;
        const std::uint64_t section_address =
            static_cast<std::uint64_t>(image_base) + virtual_address;
        if (section_address > std::numeric_limits<std::uint32_t>::max() || mapped_size == 0U)
        {
            return Refuse("PE section address overflows the guest address space");
        }
        const GuestAddress section_base = static_cast<GuestAddress>(section_address);
        layout.sections.push_back({SectionName(source, section), section_base, mapped_size, code});
        if (raw_size != 0U)
        {
            std::copy_n(source.begin() + raw_offset, raw_size,
                        layout.image.begin() + virtual_address);
        }
        if (code)
        {
            code_begin = std::min(code_begin, section_address);
            code_end = std::max(code_end, section_address + mapped_size);
        }
    }

    const std::uint64_t image_end = static_cast<std::uint64_t>(image_base) + image_size;
    const std::uint64_t entry_point = static_cast<std::uint64_t>(image_base) + entry_rva;
    if (image_end > std::numeric_limits<std::uint32_t>::max() + std::uint64_t{1} ||
        code_begin == std::numeric_limits<std::uint64_t>::max() || code_end <= code_begin ||
        entry_point < code_begin || entry_point >= code_end || (entry_point & 3U) != 0U)
    {
        return Refuse("PE image has no valid executable range containing its entry point");
    }

    layout.identity.entry_point = static_cast<GuestAddress>(entry_point);
    layout.code = {static_cast<GuestAddress>(code_begin),
                   static_cast<std::uint32_t>(code_end - code_begin)};
    layout.identity.sha256 = HashBytes(layout.image);
    return {.layout = std::move(layout), .error = {}};
}

} // namespace x360port
