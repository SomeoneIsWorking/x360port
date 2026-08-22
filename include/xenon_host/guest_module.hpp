#ifndef XENON_HOST_GUEST_MODULE_HPP
#define XENON_HOST_GUEST_MODULE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace xenon_host
{

using GuestAddress = std::uint32_t;
using Sha256Digest = std::array<std::uint8_t, 32>;

// The shared host deliberately knows nothing about a generated PPC context.
// A title bridge owns the concrete ABI and adapts it behind this thunk.
using GuestEntryThunk = void (*)(void* title_context) noexcept;

struct ImageIdentity
{
    Sha256Digest sha256{};
    GuestAddress base = 0;
    std::uint32_t size = 0;
    GuestAddress entry_point = 0;
};

struct CodeRange
{
    GuestAddress base = 0;
    std::uint32_t size = 0;
};

struct FunctionMapping
{
    GuestAddress address = 0;
    GuestEntryThunk thunk = nullptr;
};

struct ImportRequirement
{
    std::string_view library;
    std::uint32_t ordinal = 0;
    std::string_view name;
};

struct ModuleDescriptor
{
    ImageIdentity image;
    CodeRange code;
    std::size_t function_count = 0;
    Sha256Digest function_map_sha256{};
    std::size_t import_count = 0;
    Sha256Digest import_manifest_sha256{};
};

class GuestModule
{
  public:
    virtual ~GuestModule() = default;

    [[nodiscard]] virtual const ModuleDescriptor& Descriptor() const noexcept = 0;
    [[nodiscard]] virtual std::span<const std::byte> ImageBytes() const noexcept = 0;
    [[nodiscard]] virtual std::span<const FunctionMapping> FunctionMap() const noexcept = 0;
    [[nodiscard]] virtual std::span<const ImportRequirement> ImportManifest() const noexcept = 0;
};

[[nodiscard]] Sha256Digest HashBytes(std::span<const std::byte> bytes) noexcept;
[[nodiscard]] Sha256Digest HashFunctionMap(std::span<const FunctionMapping> mappings) noexcept;
[[nodiscard]] Sha256Digest HashImportManifest(std::span<const ImportRequirement> imports) noexcept;

} // namespace xenon_host

#endif
