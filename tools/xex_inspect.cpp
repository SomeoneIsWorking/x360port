#include "x360port/xex_inspect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{

[[nodiscard]] std::string Hex32(std::uint32_t value)
{
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(8) << value;
    return output.str();
}

[[nodiscard]] std::string JsonString(std::string_view value)
{
    std::ostringstream output;
    output << '"';
    for (const unsigned char character : value)
    {
        switch (character)
        {
        case '"':
            output << "\\\"";
            break;
        case '\\':
            output << "\\\\";
            break;
        case '\n':
            output << "\\n";
            break;
        case '\r':
            output << "\\r";
            break;
        case '\t':
            output << "\\t";
            break;
        default:
            if (character < 0x20U)
            {
                output << "\\u" << std::hex << std::setfill('0') << std::setw(4)
                       << static_cast<unsigned>(character);
            }
            else
            {
                output << static_cast<char>(character);
            }
            break;
        }
    }
    output << '"';
    return output.str();
}

[[nodiscard]] std::string Sha256Hex(const x360port::Sha256Digest& digest)
{
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const std::uint8_t byte : digest)
    {
        output << std::setw(2) << static_cast<unsigned>(byte);
    }
    return output.str();
}

[[nodiscard]] std::vector<std::byte> ReadFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error("could not open XEX");
    }
    const std::vector<char> bytes((std::istreambuf_iterator<char>(input)),
                                  std::istreambuf_iterator<char>());
    std::vector<std::byte> result(bytes.size());
    for (std::size_t index = 0; index < bytes.size(); ++index)
    {
        result[index] = static_cast<std::byte>(static_cast<unsigned char>(bytes[index]));
    }
    return result;
}

void WriteImage(const std::filesystem::path& path, std::span<const std::byte> image)
{
    if (std::filesystem::is_symlink(std::filesystem::symlink_status(path)))
    {
        throw std::runtime_error("refusing to overwrite a symlink image output");
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        throw std::runtime_error("could not create image output");
    }
    for (const std::byte byte : image)
    {
        output.put(static_cast<char>(std::to_integer<unsigned char>(byte)));
    }
    if (!output)
    {
        throw std::runtime_error("could not write image output");
    }
}

[[nodiscard]] std::string Document(std::span<const std::byte> xex,
                                   const x360port::XexInspection& inspection)
{
    const auto& execution = inspection.execution;
    const auto& layout = inspection.image;
    std::ostringstream output;
    output << "{\n"
           << "  \"schema\": 1,\n"
           << "  \"format\": \"XEX2\",\n"
           << "  \"xex\": {\"sha256\": " << JsonString(Sha256Hex(x360port::HashBytes(xex)))
           << ", \"size\": " << xex.size() << "},\n"
           << "  \"execution\": {\n"
           << "    \"title_id\": " << JsonString(Hex32(execution.title_id)) << ",\n"
           << "    \"media_id\": " << JsonString(Hex32(execution.media_id)) << ",\n"
           << "    \"version\": " << JsonString(Hex32(execution.version)) << ",\n"
           << "    \"base_version\": " << JsonString(Hex32(execution.base_version)) << ",\n"
           << "    \"platform\": " << static_cast<unsigned>(execution.platform) << ",\n"
           << "    \"executable_table\": " << static_cast<unsigned>(execution.executable_table)
           << ",\n"
           << "    \"disc_number\": " << static_cast<unsigned>(execution.disc_number) << ",\n"
           << "    \"disc_count\": " << static_cast<unsigned>(execution.disc_count) << ",\n"
           << "    \"savegame_id\": " << JsonString(Hex32(execution.savegame_id)) << "\n"
           << "  },\n"
           << "  \"image\": {\"sha256\": "
           << JsonString(Sha256Hex(x360port::HashBytes(inspection.normalized_image)))
           << ", \"base\": " << JsonString(Hex32(layout.identity.base))
           << ", \"size\": " << inspection.normalized_image.size()
           << ", \"entry\": " << JsonString(Hex32(layout.identity.entry_point)) << "},\n"
           << "  \"sections\": [\n";
    for (std::size_t index = 0; index < layout.sections.size(); ++index)
    {
        const auto& section = layout.sections[index];
        output << "    {\"name\": " << JsonString(section.name)
               << ", \"base\": " << JsonString(Hex32(section.base))
               << ", \"size\": " << section.size
               << ", \"code\": " << (section.code ? "true" : "false") << "}";
        output << (index + 1U == layout.sections.size() ? "\n" : ",\n");
    }
    output << "  ],\n  \"imports\": [\n";
    for (std::size_t index = 0; index < inspection.imports.size(); ++index)
    {
        const auto& imported = inspection.imports[index];
        output << "    {\"kind\": "
               << JsonString(imported.kind == x360port::ImportKind::Function ? "function"
                                                                             : "variable")
               << ", \"library\": " << JsonString(imported.library)
               << ", \"ordinal\": " << imported.ordinal
               << ", \"name\": " << JsonString(imported.name)
               << ", \"address\": " << JsonString(Hex32(imported.address))
               << ", \"record_address\": " << JsonString(Hex32(imported.record_address)) << "}";
        output << (index + 1U == inspection.imports.size() ? "\n" : ",\n");
    }
    output << "  ],\n  \"helpers\": {\n";
    constexpr std::array<std::string_view, 8> helper_names = {
        "restgprlr_14", "savegprlr_14", "restfpr_14", "savefpr_14",
        "restvmx_14",   "savevmx_14",   "restvmx_64", "savevmx_64"};
    for (std::size_t index = 0; index < helper_names.size(); ++index)
    {
        output << "    " << JsonString(helper_names[index]) << ": [";
        const auto& addresses = inspection.helpers[index];
        for (std::size_t address_index = 0; address_index < addresses.size(); ++address_index)
        {
            if (address_index != 0U)
            {
                output << ", ";
            }
            output << JsonString(Hex32(addresses[address_index]));
        }
        output << "]" << (index + 1U == helper_names.size() ? "\n" : ",\n");
    }
    output << "  }\n}\n";
    return output.str();
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2 || argc > 4 || (argc == 4 && std::string_view(argv[2]) != "--image-out"))
    {
        std::cerr << "usage: x360-xex-inspect <default.xex> [--image-out path]\n";
        return 2;
    }
    try
    {
        const std::filesystem::path xex_path = argv[1];
        const std::vector<std::byte> xex = ReadFile(xex_path);
        const x360port::XexInspectionResult result = x360port::InspectXex(xex);
        if (!result)
        {
            std::cerr << "x360-xex-inspect: refusing: " << result.error << '\n';
            return 1;
        }
        if (argc == 4)
        {
            WriteImage(argv[3], result.inspection.normalized_image);
        }
        std::cout << Document(xex, result.inspection);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "x360-xex-inspect: refusing: " << error.what() << '\n';
        return 1;
    }
}
