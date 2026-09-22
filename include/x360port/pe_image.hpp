#ifndef X360PORT_PE_IMAGE_HPP
#define X360PORT_PE_IMAGE_HPP

#include "x360port/module_contract.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace x360port
{

struct PeSection
{
    std::string name;
    GuestAddress base = 0;
    std::uint32_t size = 0;
    bool code = false;
};

// Where an Xbox 360 image's sections sit once loaded, and what executes.
struct PeImageLayout
{
    ImageIdentity identity;
    CodeRange code;
    std::vector<PeSection> sections;
};

struct PeImageLayoutResult
{
    PeImageLayout layout;
    std::string error;

    [[nodiscard]] explicit operator bool() const noexcept { return error.empty(); }
};

// Describes an Xbox 360 image as the XEX loader leaves it at its base
// address: the decompressed basefile, copied flat. The loader does not move
// sections to their PE VirtualAddress; each section's bytes stay at its
// PointerToRawData, which a retail image can place well below the section's
// VirtualAddress. The identity
// digest covers `image` itself. This neither authenticates nor decrypts the
// XEX container; the title/provisioning owners authenticate the bytes first.
[[nodiscard]] PeImageLayoutResult DescribeLoadedPeImage(std::span<const std::byte> image);

} // namespace x360port

#endif
