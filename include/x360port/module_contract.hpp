#ifndef X360PORT_MODULE_CONTRACT_HPP
#define X360PORT_MODULE_CONTRACT_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace x360port
{

using GuestAddress = std::uint32_t;
using Sha256Digest = std::array<std::uint8_t, 32>;

inline constexpr std::uint32_t kGuestImageBaseAlignment = 64U * 1024U;

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

enum class ImportKind : std::uint8_t
{
    Function = 0,
    Variable = 1,
};

struct ImportRequirement
{
    ImportKind kind = ImportKind::Function;
    std::string_view library;
    std::uint32_t ordinal = 0;
    std::string_view name;
    GuestAddress address = 0;
    GuestAddress record_address = 0;
};

struct ModuleDescriptor
{
    ImageIdentity image;
    CodeRange code;
    std::size_t import_count = 0;
    Sha256Digest import_manifest_sha256{};
};

class GuestModule
{
  public:
    virtual ~GuestModule() = default;

    [[nodiscard]] virtual const ModuleDescriptor& Descriptor() const noexcept = 0;
    [[nodiscard]] virtual std::span<const std::byte> ImageBytes() const noexcept = 0;
    [[nodiscard]] virtual std::span<const ImportRequirement> ImportManifest() const noexcept = 0;
};

[[nodiscard]] Sha256Digest HashBytes(std::span<const std::byte> bytes) noexcept;
[[nodiscard]] Sha256Digest HashImportManifest(std::span<const ImportRequirement> imports) noexcept;

} // namespace x360port

#endif
