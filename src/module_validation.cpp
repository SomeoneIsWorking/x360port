#include "x360port/validation.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace x360port
{
namespace
{

[[nodiscard]] ValidationResult Refuse(ValidationError error, std::string detail)
{
    return {.error = error, .detail = std::move(detail)};
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

ValidationResult ValidateModule(const GuestModule& module) noexcept
{
    const ModuleDescriptor& descriptor = module.Descriptor();
    if (std::ranges::all_of(descriptor.image.sha256, [](std::uint8_t byte) { return byte == 0; }))
    {
        return Refuse(ValidationError::InvalidImageDigest,
                      "module image identity has an empty SHA-256 digest");
    }
    const std::span<const std::byte> image = module.ImageBytes();
    if (image.size() != descriptor.image.size)
    {
        return Refuse(ValidationError::ImageSizeMismatch,
                      "module image byte count does not match its sealed size");
    }
    const std::uint64_t image_end =
        static_cast<std::uint64_t>(descriptor.image.base) + descriptor.image.size;
    if (image_end > std::numeric_limits<std::uint32_t>::max() + std::uint64_t{1})
    {
        return Refuse(ValidationError::ImageAddressOverflow,
                      "module image exceeds the Xbox 360 32-bit address space");
    }
    if (HashBytes(image) != descriptor.image.sha256)
    {
        return Refuse(ValidationError::ImageDigestMismatch,
                      "module image bytes do not match the sealed SHA-256 digest");
    }

    const CodeRange code = descriptor.code;
    const std::uint64_t code_end = static_cast<std::uint64_t>(code.base) + code.size;
    if (code.size == 0 || (code.base & 3U) != 0 || (code.size & 3U) != 0 ||
        code.base < descriptor.image.base || code_end > image_end)
    {
        return Refuse(ValidationError::InvalidCodeRange,
                      "code range must be aligned, non-empty, and contained by the image");
    }
    if ((descriptor.image.entry_point & 3U) != 0 || !IsInside(descriptor.image.entry_point, code))
    {
        return Refuse(ValidationError::EntryPointOutsideCode,
                      "image entry point is not an aligned address in the code range");
    }

    const std::span<const ImportRequirement> imports = module.ImportManifest();
    if (imports.size() != descriptor.import_count)
    {
        return Refuse(ValidationError::ImportCountMismatch,
                      "import-manifest count does not match the sealed count");
    }
    for (std::size_t index = 0; index < imports.size(); ++index)
    {
        const ImportRequirement& import = imports[index];
        const bool record_inside_image =
            import.record_address >= descriptor.image.base &&
            static_cast<std::uint64_t>(import.record_address) + sizeof(std::uint32_t) <= image_end;
        const bool function_address =
            import.kind == ImportKind::Function && IsInside(import.address, code);
        const bool variable_address =
            import.kind == ImportKind::Variable && import.address == import.record_address &&
            import.address >= descriptor.image.base && import.address < image_end;
        if (import.library.size() > std::numeric_limits<std::uint32_t>::max() ||
            import.name.size() > std::numeric_limits<std::uint32_t>::max() ||
            !IsValidLibrary(import.library) || import.ordinal == 0 ||
            !IsValidImportName(import.name) || (import.address & 3U) != 0 ||
            (import.record_address & 3U) != 0 || !record_inside_image ||
            (!function_address && !variable_address))
        {
            return Refuse(ValidationError::InvalidImport,
                          "import contains invalid identity, kind, or guest addresses");
        }
        if (index != 0 && !ImportLess(imports[index - 1U], import))
        {
            return Refuse(ValidationError::UnsortedImportManifest,
                          "import manifest must be strictly sorted by library and ordinal");
        }
    }
    if (HashImportManifest(imports) != descriptor.import_manifest_sha256)
    {
        return Refuse(ValidationError::ImportManifestDigestMismatch,
                      "import manifest does not match the sealed SHA-256");
    }
    return {};
}

ValidationResult ValidateImports(const GuestModule& module,
                                 std::span<const ImportBinding> bindings) noexcept
{
    const std::span<const ImportRequirement> required = module.ImportManifest();
    if (bindings.size() != required.size())
    {
        return Refuse(ValidationError::ImportBindingCountMismatch,
                      "adapter binding count does not exactly cover the import manifest");
    }
    for (std::size_t index = 0; index < required.size(); ++index)
    {
        if (bindings[index].library != required[index].library ||
            bindings[index].ordinal != required[index].ordinal)
        {
            return Refuse(ValidationError::ImportBindingMismatch,
                          "adapter import binding does not match the manifest at the same index");
        }
        if (bindings[index].kind != required[index].kind)
        {
            return Refuse(ValidationError::ImportBindingKindMismatch,
                          "adapter import binding kind does not match the manifest");
        }
        const bool valid_function = bindings[index].kind == ImportKind::Function &&
                                    bindings[index].function_handler != nullptr &&
                                    bindings[index].variable_resolver == nullptr;
        const bool valid_variable = bindings[index].kind == ImportKind::Variable &&
                                    bindings[index].function_handler == nullptr &&
                                    bindings[index].variable_resolver != nullptr;
        if (!valid_function && !valid_variable)
        {
            return Refuse(ValidationError::ImportBindingCallbackMismatch,
                          "adapter import binding has the wrong callback shape");
        }
    }
    return {};
}

} // namespace x360port
