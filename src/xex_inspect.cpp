#include "x360port/xex_inspect.hpp"

#include "xenia/base/platform.h"
#include "xenia/cpu/export_resolver.h"
#include "xenia/cpu/processor.h"
#include "xenia/cpu/xex_module.h"
#include "xenia/memory.h"

#include "xenia_backend.hpp"
#include "xex_helpers.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>

namespace x360port
{
namespace
{

constexpr std::uint32_t kXex2 = 0x58455832U;
constexpr std::size_t kXexHeaderPrefix = 0x18U;
constexpr std::size_t kSecurityPrefix = 0x184U;
constexpr std::size_t kPageDescriptorSize = 0x18U;
constexpr std::size_t kFileFormatMinimum = 0x14U;
constexpr std::size_t kImportHeaderMinimum = 0x0CU;
constexpr std::size_t kImportLibraryMinimum = 0x28U;

struct OptionalHeader
{
    std::uint32_t key = 0;
    std::uint32_t value = 0;
    std::size_t offset = 0;
    bool inline_value = false;
};

struct HeaderGeometry
{
    std::size_t size = 0;
    std::uint32_t count = 0;
};

struct ImageBounds
{
    std::uint32_t base = 0;
    std::size_t size = 0;
};

struct ImportContext
{
    HeaderGeometry header;
    ImageBounds image;
};

[[nodiscard]] XexInspectionResult Refuse(std::string message)
{
    return {.inspection = {}, .error = std::move(message)};
}

[[nodiscard]] bool HasBytes(std::span<const std::byte> bytes, std::size_t offset,
                            std::size_t count) noexcept
{
    return offset <= bytes.size() && count <= bytes.size() - offset;
}

[[nodiscard]] std::uint16_t ReadU16Be(std::span<const std::byte> bytes, std::size_t offset) noexcept
{
    return static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset]) << 8U) |
           static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset + 1U]));
}

[[nodiscard]] std::uint32_t ReadU32Be(std::span<const std::byte> bytes, std::size_t offset) noexcept
{
    return (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset])) << 24U) |
           (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 1U])) << 16U) |
           (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 2U])) << 8U) |
           static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 3U]));
}

[[nodiscard]] std::string ReadCString(std::span<const std::byte> bytes, std::size_t offset,
                                      std::size_t limit)
{
    if (offset >= limit)
    {
        return {};
    }
    const std::size_t end = std::find_if(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                                         bytes.begin() + static_cast<std::ptrdiff_t>(limit),
                                         [](std::byte value) { return value == std::byte{0}; }) -
                            bytes.begin();
    if (end == limit)
    {
        return {};
    }
    std::string result;
    result.reserve(end - offset);
    for (std::size_t index = offset; index < end; ++index)
    {
        result.push_back(static_cast<char>(std::to_integer<std::uint8_t>(bytes[index])));
    }
    return result;
}

[[nodiscard]] std::optional<OptionalHeader> FindOptional(std::span<const std::byte> xex,
                                                         std::uint32_t key, HeaderGeometry header)
{
    for (std::uint32_t index = 0; index < header.count; ++index)
    {
        const std::size_t offset = kXexHeaderPrefix + static_cast<std::size_t>(index) * 8U;
        if (!HasBytes(xex, offset, 8U))
        {
            return std::nullopt;
        }
        const std::uint32_t candidate = ReadU32Be(xex, offset);
        if (candidate != key)
        {
            continue;
        }
        const std::uint32_t value = ReadU32Be(xex, offset + 4U);
        if ((key & 0xFFU) == 0U || (key & 0xFFU) == 1U)
        {
            return OptionalHeader{key, value, offset + 4U, true};
        }
        if (value >= header.size || !HasBytes(xex, value, 4U))
        {
            return std::nullopt;
        }
        return OptionalHeader{key, value, value, false};
    }
    return std::nullopt;
}

[[nodiscard]] bool ValidateHeader(std::span<const std::byte> xex, HeaderGeometry& header,
                                  std::size_t& security_offset, std::string& error)
{
    if (!HasBytes(xex, 0, kXexHeaderPrefix))
    {
        error = "XEX is shorter than its fixed header";
        return false;
    }
    if (ReadU32Be(xex, 0) != kXex2)
    {
        error = "only XEX2 is accepted by the checked inspector";
        return false;
    }
    header.size = ReadU32Be(xex, 8U);
    security_offset = ReadU32Be(xex, 0x10U);
    header.count = ReadU32Be(xex, 0x14U);
    if (header.size < kXexHeaderPrefix || header.size > xex.size() ||
        header.count > (header.size - kXexHeaderPrefix) / 8U)
    {
        error = "XEX optional-header table exceeds the source bytes";
        return false;
    }
    if (!HasBytes(xex, security_offset, kSecurityPrefix))
    {
        error = "XEX security header is truncated";
        return false;
    }
    const std::uint32_t security_header_size = ReadU32Be(xex, security_offset);
    const std::uint32_t page_count = ReadU32Be(xex, security_offset + 0x180U);
    const std::uint64_t descriptor_bytes =
        static_cast<std::uint64_t>(page_count) * kPageDescriptorSize;
    if (security_header_size < kSecurityPrefix ||
        descriptor_bytes > std::numeric_limits<std::size_t>::max() - kSecurityPrefix ||
        !HasBytes(xex, security_offset,
                  kSecurityPrefix + static_cast<std::size_t>(descriptor_bytes)) ||
        security_header_size > xex.size() - security_offset)
    {
        error = "XEX security page descriptors exceed the source bytes";
        return false;
    }
    const std::uint32_t image_size = ReadU32Be(xex, security_offset + 4U);
    const std::uint32_t load_address = ReadU32Be(xex, security_offset + 0x110U);
    if (image_size == 0U || load_address == 0U ||
        static_cast<std::uint64_t>(load_address) + image_size > (std::uint64_t{1} << 32U))
    {
        error = "XEX security image geometry is invalid";
        return false;
    }

    const auto file_format = FindOptional(xex, 0x000003FFU, header);
    if (!file_format.has_value() || file_format->inline_value ||
        !HasBytes(xex, file_format->offset, kFileFormatMinimum))
    {
        error = "XEX has no complete file-format descriptor";
        return false;
    }
    const std::uint32_t file_info_size = ReadU32Be(xex, file_format->offset);
    if (file_info_size < kFileFormatMinimum || file_info_size > header.size - file_format->offset)
    {
        error = "XEX file-format descriptor exceeds the header";
        return false;
    }
    const std::uint32_t encryption = ReadU16Be(xex, file_format->offset + 4U);
    const std::uint32_t compression = ReadU16Be(xex, file_format->offset + 6U);
    if (encryption > 1U || compression > 2U)
    {
        error = "XEX uses an unsupported encryption or compression mode";
        return false;
    }
    if (compression == 2U && ReadU32Be(xex, file_format->offset + 8U) == 0U)
    {
        error = "XEX normal compression has no LZX window";
        return false;
    }
    if (header.size == xex.size())
    {
        error = "XEX has no encrypted or compressed image payload";
        return false;
    }
    if (encryption == 1U && ((xex.size() - header.size) & 15U) != 0U)
    {
        error = "XEX encrypted image payload is not AES-block aligned";
        return false;
    }
    return true;
}

[[nodiscard]] std::string OrdinalName(std::uint32_t ordinal)
{
    constexpr char digits[] = "0123456789abcdef";
    std::string result = "ordinal_";
    for (int shift = 28; shift >= 0; shift -= 4)
    {
        result.push_back(digits[(ordinal >> shift) & 0xFU]);
    }
    return result;
}

[[nodiscard]] bool InImage(std::uint32_t address, ImageBounds image, std::size_t bytes) noexcept
{
    if (address < image.base)
    {
        return false;
    }
    const std::uint64_t offset = static_cast<std::uint64_t>(address) - image.base;
    return offset <= image.size && bytes <= image.size - static_cast<std::size_t>(offset);
}

[[nodiscard]] std::string BaseLibraryName(std::string name)
{
    const std::size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos)
    {
        name.erase(0, slash + 1U);
    }
    return name;
}

void WriteU32Le(std::span<std::byte> bytes, std::size_t offset, std::uint32_t value)
{
    bytes[offset] = static_cast<std::byte>(value & 0xffU);
    bytes[offset + 1U] = static_cast<std::byte>((value >> 8U) & 0xffU);
    bytes[offset + 2U] = static_cast<std::byte>((value >> 16U) & 0xffU);
    bytes[offset + 3U] = static_cast<std::byte>((value >> 24U) & 0xffU);
}

void NormalizeFunctionStub(std::span<std::byte> image, std::size_t offset)
{
    constexpr std::array<std::uint8_t, 16> kFunctionStub = {
        0x60U, 0x00U, 0x00U, 0x00U, 0x60U, 0x00U, 0x00U, 0x00U,
        0x60U, 0x00U, 0x00U, 0x00U, 0x4eU, 0x80U, 0x00U, 0x20U,
    };
    for (std::size_t index = 0; index < kFunctionStub.size(); ++index)
    {
        image[offset + index] = static_cast<std::byte>(kFunctionStub[index]);
    }
}

[[nodiscard]] bool ParseImports(std::span<const std::byte> header, std::span<std::byte> image,
                                ImportContext context, std::vector<XexImport>& imports,
                                std::string& error)
{
    const auto import_header = FindOptional(header, 0x000103FFU, context.header);
    if (!import_header.has_value() || import_header->inline_value ||
        !HasBytes(header, import_header->offset, kImportHeaderMinimum))
    {
        error = "XEX has no complete import-library descriptor";
        return false;
    }
    const std::size_t offset = import_header->offset;
    const std::uint32_t option_size = ReadU32Be(header, offset);
    const std::uint32_t string_size = ReadU32Be(header, offset + 4U);
    const std::uint32_t library_count = ReadU32Be(header, offset + 8U);
    if (option_size < kImportHeaderMinimum || option_size > context.header.size - offset ||
        string_size > option_size - kImportHeaderMinimum || library_count == 0U)
    {
        error = "XEX import-library string table is invalid";
        return false;
    }
    const std::size_t string_offset = offset + kImportHeaderMinimum;
    const std::size_t library_offset = string_offset + string_size;
    if (library_offset > offset + option_size)
    {
        error = "XEX import-library table starts outside its descriptor";
        return false;
    }
    std::size_t cursor = library_offset;
    for (std::uint32_t library_index = 0; library_index < library_count; ++library_index)
    {
        if (!HasBytes(header, cursor, kImportLibraryMinimum) ||
            cursor + kImportLibraryMinimum > offset + option_size)
        {
            error = "XEX import-library record is truncated";
            return false;
        }
        const std::uint32_t library_size = ReadU32Be(header, cursor);
        const std::uint16_t name_index = ReadU16Be(header, cursor + 0x24U);
        const std::uint16_t import_count = ReadU16Be(header, cursor + 0x26U);
        const std::uint64_t table_bytes = static_cast<std::uint64_t>(import_count) * 4U;
        if (library_size < kImportLibraryMinimum || table_bytes > library_size - 0x28U ||
            library_size > offset + option_size - cursor || name_index >= string_size)
        {
            error = "XEX import-library record has invalid bounds";
            return false;
        }
        std::string library =
            ReadCString(header, string_offset + name_index, string_offset + string_size);
        if (library.empty())
        {
            error = "XEX import-library name is missing or unterminated";
            return false;
        }
        library = BaseLibraryName(std::move(library));
        struct RawImport
        {
            std::uint32_t ordinal = 0;
            std::uint32_t address = 0;
            std::uint32_t value = 0;
            std::uint32_t type = 0;
        };
        std::vector<RawImport> raw_imports;
        raw_imports.reserve(import_count);
        for (std::uint32_t import_index = 0; import_index < import_count; ++import_index)
        {
            const std::size_t record_entry =
                cursor + 0x28U + static_cast<std::size_t>(import_index) * 4U;
            const std::uint32_t record = ReadU32Be(header, record_entry);
            if (record == 0U || (record & 3U) != 0U || !InImage(record, context.image, 4U))
            {
                error = "XEX import record points outside the normalized image";
                return false;
            }
            const std::size_t record_offset = static_cast<std::size_t>(record - context.image.base);
            const std::uint32_t value = ReadU32Be(image, record_offset);
            const std::uint32_t type = value >> 24U;
            const std::uint32_t ordinal = value & 0xFFFFU;
            if (type != 0U && type != 1U)
            {
                error = "XEX import records have an unsupported or unpaired type";
                return false;
            }
            raw_imports.push_back({ordinal, record, value, type});
        }
        for (std::size_t import_index = 0; import_index < raw_imports.size(); ++import_index)
        {
            const RawImport& variable = raw_imports[import_index];
            if (variable.type != 0U)
            {
                error = "XEX function thunk lacks an adjacent variable record";
                return false;
            }
            WriteU32Le(image, static_cast<std::size_t>(variable.address - context.image.base),
                       variable.value);
            if (import_index + 1U < raw_imports.size() && raw_imports[import_index + 1U].type == 1U)
            {
                const RawImport& function = raw_imports[++import_index];
                if (function.ordinal != variable.ordinal ||
                    !InImage(function.address, context.image, 16U))
                {
                    error = "XEX adjacent import record and function thunk disagree";
                    return false;
                }
                NormalizeFunctionStub(
                    image, static_cast<std::size_t>(function.address - context.image.base));
                imports.push_back({ImportKind::Function, library, function.ordinal,
                                   OrdinalName(function.ordinal), function.address,
                                   variable.address});
                continue;
            }
            imports.push_back({ImportKind::Variable, library, variable.ordinal,
                               OrdinalName(variable.ordinal), variable.address, variable.address});
        }
        cursor += library_size;
    }
    if (cursor != offset + option_size)
    {
        error = "XEX import-library descriptor has trailing or overlapping bytes";
        return false;
    }
    return true;
}

} // namespace

XexInspectionResult InspectXex(std::span<const std::byte> xex)
{
    HeaderGeometry header;
    std::size_t security_offset = 0;
    std::string error;
    if (!ValidateHeader(xex, header, security_offset, error))
    {
        return Refuse(std::move(error));
    }

    xe::Memory memory;
    if (!memory.Initialize())
    {
        return Refuse("Xenia refused its guest address-space mapping");
    }
    xe::cpu::ExportResolver exports;
    xe::cpu::Processor processor(&memory, &exports);
    if (!processor.Setup(CreateXeniaHostBackend()))
    {
        return Refuse("Xenia refused the host dynarec backend");
    }
    xe::cpu::XexModule module(&processor, nullptr);
    if (!module.Load("checked-xex", "checked-xex", xex.data(), xex.size()))
    {
        return Refuse("Xenia rejected the XEX after checked header validation");
    }

    const std::uint32_t base = module.base_address();
    const std::uint32_t image_size = module.image_size();
    if (base == 0U || image_size == 0U || !HasBytes(xex, 0, header.size))
    {
        static_cast<void>(module.Unload());
        return Refuse("Xenia produced invalid normalized image geometry");
    }
    const auto* normalized = memory.TranslateVirtual(base);
    if (normalized == nullptr)
    {
        static_cast<void>(module.Unload());
        return Refuse("Xenia did not expose the normalized guest image");
    }
    XexInspection inspection;
    inspection.normalized_image.resize(image_size);
    std::memcpy(inspection.normalized_image.data(), normalized, image_size);
    const auto* execution = module.opt_execution_info();
    if (execution == nullptr)
    {
        static_cast<void>(module.Unload());
        return Refuse("XEX has no execution metadata");
    }
    inspection.execution = {
        execution->title_id,           execution->media_id,   execution->version_value,
        execution->base_version_value, execution->platform,   execution->executable_table,
        execution->disc_number,        execution->disc_count, execution->savegame_id,
    };
    const ImportContext import_context{header, {base, inspection.normalized_image.size()}};
    if (!ParseImports(xex.first(header.size), inspection.normalized_image, import_context,
                      inspection.imports, error))
    {
        static_cast<void>(module.Unload());
        return Refuse(std::move(error));
    }
    const PeImageLayoutResult mapped = MapPeImage(inspection.normalized_image);
    if (!mapped)
    {
        static_cast<void>(module.Unload());
        return Refuse("normalized XEX image is not a supported Xbox 360 PE: " + mapped.error);
    }
    inspection.image = mapped.layout;
    ScanXexHelpers(inspection.image, inspection.helpers);
    static_cast<void>(module.Unload());
    return {.inspection = std::move(inspection), .error = {}};
}

} // namespace x360port
