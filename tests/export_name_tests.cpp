#include "x360port/export_names.hpp"
#include "x360port/import_claims.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
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

void RecordingHandler(GuestImportContext&, void*) noexcept {}

void SecondHandler(GuestImportContext&, void*) noexcept {}

// Resolution refuses at composition, so each defect is checked for the reason it
// reports and not merely for failing.
void RequireClaimRefusal(std::span<const ImportClaim> claims, std::string_view expected_detail,
                         std::string_view description)
{
    ImportClaimTable table;
    const RuntimeFailure failure = table.Resolve(claims);
    Require(failure.error == RuntimeError::ImportValidationFailed &&
                failure.detail.find(expected_detail) != std::string::npos,
            description);
    Require(table.size() == 0U, "a refused claim table retained resolved claims");
}

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

    // A claim table that resolved nothing would pass every refusal below, so the
    // accepted case and its applied count are asserted first.
    const std::array<ImportClaim, 2> accepted{
        ImportClaim{.library = ExportNames::Library::Kernel,
                    .export_name = "KeQueryPerformanceFrequency",
                    .handler = RecordingHandler},
        ImportClaim{.library = ExportNames::Library::Xam,
                    .export_name = "XGetAVPack",
                    .handler = SecondHandler}};
    ImportClaimTable table;
    Require(!table.Resolve(accepted) && table.size() == 2U,
            "two distinct named exports did not resolve to claims");
    ImportRequirement av_pack_requirement{.kind = ImportKind::Function,
                                          .library = "xam.xex",
                                          .ordinal = kXGetAVPack,
                                          .name = "XGetAVPack"};
    ImportBinding binding{};
    table.Apply(av_pack_requirement, binding);
    Require(binding.function_handler == SecondHandler && table.applied() == 1U,
            "a resolved claim did not install its handler for the matching import");
    ImportRequirement unclaimed{.kind = ImportKind::Function,
                                .library = "xam.xex",
                                .ordinal = kXGetAVPack + 1U,
                                .name = "Unclaimed"};
    ImportBinding untouched{};
    table.Apply(unclaimed, untouched);
    Require(untouched.function_handler == nullptr && table.applied() == 1U,
            "an unclaimed import was given a handler");

    const std::array<ImportClaim, 1> misspelled{
        ImportClaim{.library = ExportNames::Library::Kernel,
                    .export_name = "KeQueryPerformanceFrequncy",
                    .handler = RecordingHandler}};
    RequireClaimRefusal(misspelled, "is not declared by that library",
                        "a misspelled claim resolved instead of refusing");
    const std::array<ImportClaim, 2> duplicated{ImportClaim{.library = ExportNames::Library::Xam,
                                                            .export_name = "XGetAVPack",
                                                            .handler = RecordingHandler},
                                                ImportClaim{.library = ExportNames::Library::Xam,
                                                            .export_name = "XGetAVPack",
                                                            .handler = SecondHandler}};
    RequireClaimRefusal(duplicated, "claimed by more than one service",
                        "two services claiming one export did not refuse");
    const std::array<ImportClaim, 1> variable_export{
        ImportClaim{.library = ExportNames::Library::Kernel,
                    .export_name = "ExEventObjectType",
                    .handler = RecordingHandler}};
    RequireClaimRefusal(variable_export, "is a variable export",
                        "a handler claimed on a variable export did not refuse");
    const std::array<ImportClaim, 1> handlerless{
        ImportClaim{.library = ExportNames::Library::Xam, .export_name = "XGetAVPack"}};
    RequireClaimRefusal(handlerless, "without a handler", "a claim with no handler did not refuse");

    std::cout << "export name contract: " << kernel_exports << " kernel and " << xam_exports
              << " XAM exports resolve by ordinal and by name, and unknown libraries, "
                 "ordinals, and names refuse, and claims refuse a misspelled, duplicated, "
                 "variable, or handlerless export\n";
    return 0;
}
