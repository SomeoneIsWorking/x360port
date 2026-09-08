#ifndef X360PORT_XEX_INSPECT_HPP
#define X360PORT_XEX_INSPECT_HPP

#include "x360port/pe_image.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace x360port
{

struct XexExecutionInfo
{
    std::uint32_t title_id = 0;
    std::uint32_t media_id = 0;
    std::uint32_t version = 0;
    std::uint32_t base_version = 0;
    std::uint8_t platform = 0;
    std::uint8_t executable_table = 0;
    std::uint8_t disc_number = 0;
    std::uint8_t disc_count = 0;
    std::uint32_t savegame_id = 0;
};

struct XexImport
{
    ImportKind kind = ImportKind::Function;
    std::string library;
    std::uint32_t ordinal = 0;
    std::string name;
    GuestAddress address = 0;
    GuestAddress record_address = 0;
};

struct XexInspection
{
    std::vector<std::byte> normalized_image;
    XexExecutionInfo execution;
    PeImageLayout image;
    std::vector<XexImport> imports;
    std::array<std::vector<GuestAddress>, 8> helpers;
};

struct XexInspectionResult
{
    XexInspection inspection;
    std::string error;

    [[nodiscard]] explicit operator bool() const noexcept { return error.empty(); }
};

// Authenticates and expands one XEX2 image through the pinned Xenia loader.
// The normalized image is the decrypted/decompressed XEX image container that
// MapPeImage consumes; it is not a runtime cache and must remain user-owned.
[[nodiscard]] XexInspectionResult InspectXex(std::span<const std::byte> xex);

} // namespace x360port

#endif
