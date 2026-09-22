#include "x360port/export_names.hpp"

#include <algorithm>
#include <array>
#include <span>

namespace x360port
{
namespace
{

// The vendored ordinal tables spell each export's kind with these tokens; the
// macro below expands them into this framework's own kind so the table needs no
// second translation step.
constexpr ImportKind kFunction = ImportKind::Function;
constexpr ImportKind kVariable = ImportKind::Variable;

#define XE_EXPORT(module, ordinal, name, kind)                                                     \
    ExportNames::Export { #name, (ordinal), (kind) }

// Built-in arrays rather than std::array: class-template argument deduction
// folds one expression per element, and these tables exceed the 256-term
// nesting limit some supported toolchains enforce.
constexpr ExportNames::Export kKernelExports[]{
#include "xenia/kernel/xboxkrnl/xboxkrnl_table.inc"
};

constexpr ExportNames::Export kXamExports[]{
#include "xenia/kernel/xam/xam_table.inc"
};

#undef XE_EXPORT

[[nodiscard]] std::span<const ExportNames::Export> Exports(ExportNames::Library library) noexcept
{
    switch (library)
    {
    case ExportNames::Library::Kernel:
        return kKernelExports;
    case ExportNames::Library::Xam:
        return kXamExports;
    }
    return {};
}

} // namespace

std::optional<ExportNames::Library> ExportNames::ParseLibrary(std::string_view name) noexcept
{
    if (name == kernel_library_name)
    {
        return Library::Kernel;
    }
    if (name == xam_library_name)
    {
        return Library::Xam;
    }
    return std::nullopt;
}

std::optional<ExportNames::Export> ExportNames::Find(Library library,
                                                     std::uint32_t ordinal) noexcept
{
    const std::span<const Export> exports = Exports(library);
    const auto found = std::ranges::find(exports, ordinal, &Export::ordinal);
    return found == exports.end() ? std::nullopt : std::optional{*found};
}

std::optional<ExportNames::Export> ExportNames::Find(Library library,
                                                     std::string_view name) noexcept
{
    const std::span<const Export> exports = Exports(library);
    const auto found = std::ranges::find(exports, name, &Export::name);
    return found == exports.end() ? std::nullopt : std::optional{*found};
}

std::size_t ExportNames::ExportCount(Library library) noexcept { return Exports(library).size(); }

} // namespace x360port
