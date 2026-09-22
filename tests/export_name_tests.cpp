#include "x360port/export_names.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace
{

using namespace x360port;

void Require(bool condition, std::string_view description)
{
    if (condition)
    {
        return;
    }
    std::cerr << "export name contract failed: " << description << '\n';
    std::exit(EXIT_FAILURE);
}

// Ordinals a consumer already binds by literal, so the table is checked against
// evidence gathered outside it rather than against itself.
constexpr std::uint32_t kNtAllocateVirtualMemory = 0xCC;
constexpr std::uint32_t kXGetAVPack = 971;
constexpr std::uint32_t kExEventObjectType = 0x0E;

} // namespace

int main()
{
    // A table that silently resolved nothing would pass every negative below,
    // so the populated case is asserted first and with a denominator.
    const std::size_t kernel_exports = ExportNames::ExportCount(ExportNames::Library::Kernel);
    const std::size_t xam_exports = ExportNames::ExportCount(ExportNames::Library::Xam);
    Require(kernel_exports > 500U && xam_exports > 500U,
            "the vendored ordinal tables did not populate both libraries");

    const auto kernel = ExportNames::ParseLibrary(ExportNames::kernel_library_name);
    const auto xam = ExportNames::ParseLibrary(ExportNames::xam_library_name);
    Require(kernel == ExportNames::Library::Kernel && xam == ExportNames::Library::Xam,
            "an import manifest library name did not parse to its library");
    Require(!ExportNames::ParseLibrary("xbdm.xex").has_value(),
            "an unknown library name parsed instead of refusing");

    const auto allocate = ExportNames::Find(ExportNames::Library::Kernel, kNtAllocateVirtualMemory);
    Require(allocate.has_value() && allocate->name == "NtAllocateVirtualMemory" &&
                allocate->kind == ImportKind::Function,
            "a known kernel function ordinal did not resolve to its export");
    const auto by_name = ExportNames::Find(ExportNames::Library::Kernel,
                                           std::string_view{"NtAllocateVirtualMemory"});
    Require(by_name.has_value() && by_name->ordinal == kNtAllocateVirtualMemory,
            "resolving the same kernel export by name disagreed with its ordinal");

    const auto av_pack = ExportNames::Find(ExportNames::Library::Xam, kXGetAVPack);
    Require(av_pack.has_value() && av_pack->name == "XGetAVPack",
            "the XAM ordinal a title already binds did not resolve to its export");

    const auto object_type = ExportNames::Find(ExportNames::Library::Kernel, kExEventObjectType);
    Require(object_type.has_value() && object_type->kind == ImportKind::Variable,
            "a kernel variable export was not reported as a variable");

    Require(!ExportNames::Find(ExportNames::Library::Kernel, 0xFFFFFFFFU).has_value(),
            "an unexported kernel ordinal resolved to an export");
    Require(
        !ExportNames::Find(ExportNames::Library::Kernel, std::string_view{"NtAllocateVirtualMemry"})
             .has_value(),
        "a misspelled export name resolved instead of refusing");
    Require(
        !ExportNames::Find(ExportNames::Library::Xam, std::string_view{"NtAllocateVirtualMemory"})
             .has_value(),
        "a kernel export resolved through the XAM library");

    std::cout << "export name contract: " << kernel_exports << " kernel and " << xam_exports
              << " XAM exports resolve by ordinal and by name, and unknown libraries, "
                 "ordinals, and names refuse\n";
    return 0;
}
