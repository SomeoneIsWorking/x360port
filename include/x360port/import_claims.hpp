#ifndef X360PORT_IMPORT_CLAIMS_HPP
#define X360PORT_IMPORT_CLAIMS_HPP

#include "x360port/export_names.hpp"
#include "x360port/runtime.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace x360port
{

// One import a host service implements, named by the export it provides rather
// than by an ordinal literal. The image's manifest carries only library and
// ordinal, so without this the only way to claim a service is to transcribe a
// number that no reader can check and no typo can fail.
struct ImportClaim
{
    ExportNames::Library library = ExportNames::Library::Kernel;
    std::string_view export_name;
    ImportFunctionHandler handler = nullptr;
    void* context = nullptr;
};

// Resolves claimed export names to their ordinals once, then applies them while
// the composition owner walks the image's manifest.
//
// A claim that names an export its library does not declare, and an export two
// services both claim, are refused at resolution. Both are composition defects
// that would otherwise present only as a service the title never reaches, which
// is indistinguishable from a service the title never calls.
class ImportClaimTable final
{
  public:
    [[nodiscard]] RuntimeFailure Resolve(std::span<const ImportClaim> claims);

    // Installs the handler for `requirement`, or leaves `binding` untouched
    // when no service claimed that import.
    void Apply(const ImportRequirement& requirement, ImportBinding& binding) noexcept;

    // How many imports of the manifest the resolved claims have installed. A
    // composition that reports zero bound services has not partially failed; it
    // has bound nothing, and the caller should say so rather than execute.
    [[nodiscard]] std::size_t applied() const noexcept { return applied_; }
    [[nodiscard]] std::size_t size() const noexcept { return resolved_.size(); }

    void Clear() noexcept;

  private:
    struct ResolvedClaim final
    {
        ExportNames::Library library = ExportNames::Library::Kernel;
        std::uint32_t ordinal = 0;
        ImportFunctionHandler handler = nullptr;
        void* context = nullptr;
    };

    [[nodiscard]] const ResolvedClaim* Find(ExportNames::Library library,
                                            std::uint32_t ordinal) const noexcept;

    std::vector<ResolvedClaim> resolved_;
    std::size_t applied_ = 0;
};

} // namespace x360port

#endif // X360PORT_IMPORT_CLAIMS_HPP
