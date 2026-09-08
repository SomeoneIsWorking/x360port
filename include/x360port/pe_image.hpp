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

struct PeImageLayout
{
    // The source digest identifies the normalized PE container emitted by the
    // checked XEX loader. The module identity digest is for the flat guest
    // image below, which is what RuntimeContext maps and executes.
    Sha256Digest source_sha256{};
    std::vector<std::byte> image;
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

// Maps an Xbox 360 PE image container into the flat guest address space
// expected by GuestModule::ImageBytes(). It does not authenticate or decrypt
// the XEX container; those title/provisioning owners must authenticate the
// source bytes before calling this function.
[[nodiscard]] PeImageLayoutResult MapPeImage(std::span<const std::byte> source);

} // namespace x360port

#endif
