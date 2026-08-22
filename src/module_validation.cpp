#include "module_validation.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace xenon_host
{
namespace
{

constexpr std::uint32_t AllCapabilityBits = static_cast<std::uint32_t>(Capability::GuestMemory) |
                                            static_cast<std::uint32_t>(Capability::KernelServices) |
                                            static_cast<std::uint32_t>(Capability::Graphics) |
                                            static_cast<std::uint32_t>(Capability::Audio) |
                                            static_cast<std::uint32_t>(Capability::Storage) |
                                            static_cast<std::uint32_t>(Capability::Networking);

[[nodiscard]] RunResult Refuse(RunError error, std::string detail)
{
    return {.error = error, .detail = std::move(detail)};
}

[[nodiscard]] bool IsZeroDigest(const Sha256Digest& digest) noexcept
{
    return std::ranges::all_of(digest, [](std::uint8_t byte) { return byte == 0; });
}

[[nodiscard]] bool IsInside(GuestAddress address, CodeRange range) noexcept
{
    const std::uint64_t begin = range.base;
    const std::uint64_t end = begin + range.size;
    return address >= begin && address < end;
}

[[nodiscard]] bool IsPortableKeyCharacter(char character) noexcept
{
    const auto value = static_cast<unsigned char>(character);
    const bool ascii_alphanumeric = (value >= 'a' && value <= 'z') ||
                                    (value >= 'A' && value <= 'Z') ||
                                    (value >= '0' && value <= '9');
    return ascii_alphanumeric || character == '_' || character == '-' || character == '.';
}

[[nodiscard]] bool IsValidLibrary(std::string_view library) noexcept
{
    if (library.empty())
    {
        return false;
    }
    return std::ranges::all_of(library, IsPortableKeyCharacter);
}

[[nodiscard]] bool IsValidImportName(std::string_view name) noexcept
{
    return std::ranges::all_of(name,
                               [](char character)
                               {
                                   const auto value = static_cast<unsigned char>(character);
                                   return value >= 0x21U && value <= 0x7eU;
                               });
}

[[nodiscard]] bool ImportLess(const ImportRequirement& left,
                              const ImportRequirement& right) noexcept
{
    if (left.library != right.library)
    {
        return left.library < right.library;
    }
    return left.ordinal < right.ordinal;
}

} // namespace

bool IsPortableKey(std::string_view key) noexcept
{
    if (key.empty() || key == "." || key == ".." || key.front() == '.' || key.back() == '.')
    {
        return false;
    }
    return std::ranges::all_of(key, IsPortableKeyCharacter);
}

bool IsValidCapabilitySet(CapabilitySet capabilities) noexcept
{
    return (capabilities.Bits() & ~AllCapabilityBits) == 0;
}

RunResult ValidateModule(const GuestModule& module) noexcept
{
    const ModuleDescriptor& descriptor = module.Descriptor();
    const std::span<const std::byte> image = module.ImageBytes();
    if (IsZeroDigest(descriptor.image.sha256))
    {
        return Refuse(RunError::InvalidImageDigest, "expected image SHA-256 is zero");
    }
    if (descriptor.image.size != image.size())
    {
        return Refuse(RunError::ImageSizeMismatch,
                      "image byte count does not equal the sealed image size");
    }
    const std::uint64_t image_end =
        static_cast<std::uint64_t>(descriptor.image.base) + descriptor.image.size;
    if (descriptor.image.size == 0 ||
        image_end > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) + 1U)
    {
        return Refuse(RunError::ImageAddressOverflow,
                      "image range is empty or exceeds 32-bit space");
    }
    if (HashBytes(image) != descriptor.image.sha256)
    {
        return Refuse(RunError::ImageDigestMismatch, "image bytes do not match the sealed SHA-256");
    }

    const CodeRange code = descriptor.code;
    const std::uint64_t code_end = static_cast<std::uint64_t>(code.base) + code.size;
    if (code.size == 0 || (code.base & 3U) != 0 || (code.size & 3U) != 0 ||
        code.base < descriptor.image.base || code_end > image_end)
    {
        return Refuse(RunError::InvalidCodeRange,
                      "code range must be aligned, non-empty, and contained by the image");
    }
    if ((descriptor.image.entry_point & 3U) != 0 || !IsInside(descriptor.image.entry_point, code))
    {
        return Refuse(RunError::EntryPointOutsideCode,
                      "image entry point is not an aligned address in the code range");
    }

    const std::span<const FunctionMapping> functions = module.FunctionMap();
    if (functions.size() != descriptor.function_count)
    {
        return Refuse(RunError::FunctionCountMismatch,
                      "function-map count does not match the sealed count");
    }
    if (functions.empty())
    {
        return Refuse(RunError::EmptyFunctionMap, "function map is empty");
    }
    bool found_entry = false;
    for (std::size_t index = 0; index < functions.size(); ++index)
    {
        const FunctionMapping& function = functions[index];
        if ((function.address & 3U) != 0 || !IsInside(function.address, code))
        {
            return Refuse(RunError::InvalidFunctionAddress,
                          "function-map address is unaligned or outside code");
        }
        if (function.thunk == nullptr)
        {
            return Refuse(RunError::NullFunctionThunk, "function-map thunk is null");
        }
        if (index != 0 && functions[index - 1U].address >= function.address)
        {
            return Refuse(RunError::UnsortedFunctionMap,
                          "function map must be strictly sorted by guest address");
        }
        found_entry = found_entry || function.address == descriptor.image.entry_point;
    }
    if (!found_entry)
    {
        return Refuse(RunError::EntryPointMissing, "function map does not contain the image entry");
    }
    if (HashFunctionMap(functions) != descriptor.function_map_sha256)
    {
        return Refuse(RunError::FunctionMapDigestMismatch,
                      "function-map addresses do not match the sealed SHA-256");
    }

    const std::span<const ImportRequirement> imports = module.ImportManifest();
    if (imports.size() != descriptor.import_count)
    {
        return Refuse(RunError::ImportCountMismatch,
                      "import-manifest count does not match the sealed count");
    }
    for (std::size_t index = 0; index < imports.size(); ++index)
    {
        const ImportRequirement& import = imports[index];
        if (import.library.size() > std::numeric_limits<std::uint32_t>::max() ||
            import.name.size() > std::numeric_limits<std::uint32_t>::max() ||
            !IsValidLibrary(import.library) || import.ordinal == 0 ||
            !IsValidImportName(import.name))
        {
            return Refuse(RunError::InvalidImport,
                          "import contains an invalid library, ordinal, or name");
        }
        if (index != 0 && !ImportLess(imports[index - 1U], import))
        {
            return Refuse(RunError::UnsortedImportManifest,
                          "import manifest must be strictly sorted by library and ordinal");
        }
    }
    if (HashImportManifest(imports) != descriptor.import_manifest_sha256)
    {
        return Refuse(RunError::ImportManifestDigestMismatch,
                      "import manifest does not match the sealed SHA-256");
    }
    return {};
}

RunResult ValidateImports(const GuestModule& module,
                          std::span<const ImportBinding> bindings) noexcept
{
    const std::span<const ImportRequirement> required = module.ImportManifest();
    if (bindings.size() != required.size())
    {
        return Refuse(RunError::ImportBindingCountMismatch,
                      "adapter binding count does not exactly cover the import manifest");
    }
    for (std::size_t index = 0; index < required.size(); ++index)
    {
        if (bindings[index].library != required[index].library ||
            bindings[index].ordinal != required[index].ordinal)
        {
            return Refuse(RunError::ImportBindingMismatch,
                          "adapter import binding does not match the manifest at the same index");
        }
        if (bindings[index].handler == nullptr)
        {
            return Refuse(RunError::NullImportHandler,
                          "adapter declared an import without an implementation");
        }
    }
    return {};
}

} // namespace xenon_host
