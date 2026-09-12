#include "xex_imports.hpp"

#include <algorithm>
#include <limits>

namespace x360port
{
namespace
{

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

[[nodiscard]] std::string BaseLibraryName(std::string name)
{
    const std::size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos)
    {
        name.erase(0, slash + 1U);
    }
    return name;
}

} // namespace

bool ParseImportLibraryNames(std::span<const std::byte> header, ImportLibraryTable table,
                             std::vector<std::string>& names, std::string& error)
{
    names.clear();
    names.reserve(table.library_count);
    const std::size_t string_limit = table.string_offset + table.string_size;
    std::size_t cursor = table.string_offset;
    for (std::uint32_t index = 0; index < table.library_count; ++index)
    {
        const std::string name = ReadCString(header, cursor, string_limit);
        if (name.empty())
        {
            error = "XEX import-library string table has a missing or unterminated name";
            return false;
        }
        names.push_back(BaseLibraryName(name));

        const std::size_t consumed = name.size() + 1U;
        if (consumed > std::numeric_limits<std::size_t>::max() - 3U)
        {
            error = "XEX import-library string table cursor overflowed";
            return false;
        }
        const std::size_t aligned = (consumed + 3U) & ~std::size_t{3U};
        if (cursor > string_limit || aligned > string_limit - cursor)
        {
            error = "XEX import-library string table exceeds its declared size";
            return false;
        }
        cursor += aligned;
    }
    return true;
}

} // namespace x360port
