#ifndef X360PORT_EXPORT_NAMES_HPP
#define X360PORT_EXPORT_NAMES_HPP

#include "x360port/module_contract.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

namespace x360port
{

// An Xbox 360 import manifest names a service only by library and ordinal, so a
// title that binds services by ordinal literal carries magic numbers no reader
// can check. Xenia's ordinal tables already record the exported name and kind
// for every ordinal, and this resolver reads those tables so a binding can name
// the service it implements.
//
// Lookups refuse rather than return a neutral value: an unknown library name or
// a misspelled export name yields nothing, so a service that claims an export
// it cannot name fails to bind loudly instead of silently binding nothing.
class ExportNames final
{
  public:
    // The libraries whose ordinals a title image can import. A library is named
    // by this type rather than by string at every lookup, so a caller cannot
    // transpose the library and the export it is asking for.
    enum class Library : std::uint8_t
    {
        Kernel,
        Xam,
    };

    struct Export final
    {
        std::string_view name;
        std::uint32_t ordinal = 0;
        ImportKind kind = ImportKind::Function;
    };

    // The library names an import manifest spells.
    static constexpr std::string_view kernel_library_name = "xboxkrnl.exe";
    static constexpr std::string_view xam_library_name = "xam.xex";

    [[nodiscard]] static std::optional<Library> ParseLibrary(std::string_view name) noexcept;

    [[nodiscard]] static std::optional<Export> Find(Library library,
                                                    std::uint32_t ordinal) noexcept;
    [[nodiscard]] static std::optional<Export> Find(Library library,
                                                    std::string_view name) noexcept;

    // Number of exports the library declares, so a caller reporting a miss can
    // say how many candidates it searched rather than only that it found none.
    [[nodiscard]] static std::size_t ExportCount(Library library) noexcept;
};

} // namespace x360port

#endif // X360PORT_EXPORT_NAMES_HPP
