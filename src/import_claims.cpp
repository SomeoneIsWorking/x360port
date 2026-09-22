#include "x360port/import_claims.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace x360port
{
namespace
{

[[nodiscard]] std::string_view LibraryName(ExportNames::Library library) noexcept
{
    return library == ExportNames::Library::Kernel ? ExportNames::kernel_library_name
                                                   : ExportNames::xam_library_name;
}

[[nodiscard]] std::string Describe(const ImportClaim& claim)
{
    std::string description{LibraryName(claim.library)};
    description += " export ";
    description += claim.export_name;
    return description;
}

} // namespace

RuntimeFailure ImportClaimTable::Resolve(std::span<const ImportClaim> claims)
{
    // Resolution either installs every claim or none: a table left holding the
    // claims that happened to precede a refusal would bind part of a service
    // whose composition the caller has already been told is invalid.
    std::vector<ResolvedClaim> resolved;
    resolved.reserve(claims.size());
    for (const ImportClaim& claim : claims)
    {
        if (claim.handler == nullptr)
        {
            return {RuntimeError::ImportValidationFailed,
                    Describe(claim) + " was claimed without a handler"};
        }
        const auto found = ExportNames::Find(claim.library, claim.export_name);
        if (!found.has_value())
        {
            return {RuntimeError::ImportValidationFailed,
                    Describe(claim) + " is not declared by that library; " +
                        std::to_string(ExportNames::ExportCount(claim.library)) +
                        " exports were searched"};
        }
        if (found->kind != ImportKind::Function)
        {
            return {RuntimeError::ImportValidationFailed,
                    Describe(claim) + " is a variable export and cannot take a handler"};
        }
        const auto duplicate = std::ranges::find_if(
            resolved, [&claim, &found](const ResolvedClaim& candidate)
            { return candidate.library == claim.library && candidate.ordinal == found->ordinal; });
        if (duplicate != resolved.end())
        {
            return {RuntimeError::ImportValidationFailed,
                    Describe(claim) + " was claimed by more than one service"};
        }
        resolved.push_back(ResolvedClaim{.library = claim.library,
                                         .ordinal = found->ordinal,
                                         .handler = claim.handler,
                                         .context = claim.context});
    }
    resolved_ = std::move(resolved);
    applied_ = 0;
    return {};
}

void ImportClaimTable::Apply(const ImportRequirement& requirement, ImportBinding& binding) noexcept
{
    if (requirement.kind != ImportKind::Function)
    {
        return;
    }
    const auto library = ExportNames::ParseLibrary(requirement.library);
    if (!library.has_value())
    {
        return;
    }
    const ResolvedClaim* claim = Find(*library, requirement.ordinal);
    if (claim == nullptr)
    {
        return;
    }
    binding.function_handler = claim->handler;
    binding.function_context = claim->context;
    ++applied_;
}

const ImportClaimTable::ResolvedClaim* ImportClaimTable::Find(ExportNames::Library library,
                                                              std::uint32_t ordinal) const noexcept
{
    const auto found = std::ranges::find_if(
        resolved_, [library, ordinal](const ResolvedClaim& candidate)
        { return candidate.library == library && candidate.ordinal == ordinal; });
    return found == resolved_.end() ? nullptr : &*found;
}

void ImportClaimTable::Clear() noexcept
{
    resolved_.clear();
    applied_ = 0;
}

} // namespace x360port
