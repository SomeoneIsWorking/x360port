#ifndef X360PORT_XEX_IMPORTS_HPP
#define X360PORT_XEX_IMPORTS_HPP

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace x360port
{

struct ImportLibraryTable
{
    std::size_t string_offset = 0;
    std::uint32_t string_size = 0;
    std::uint32_t library_count = 0;
};

[[nodiscard]] bool ParseImportLibraryNames(std::span<const std::byte> header,
                                           ImportLibraryTable table,
                                           std::vector<std::string>& names, std::string& error);

} // namespace x360port

#endif
