#ifndef X360PORT_VALIDATION_HPP
#define X360PORT_VALIDATION_HPP

#include "x360port/module_contract.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace x360port
{

using ImportFunctionHandler = void (*)(void* call_context, void* kernel_context,
                                       void* function_context) noexcept;
using ImportVariableResolver = GuestAddress (*)(void* resolution_context) noexcept;

struct ImportBinding
{
    std::string_view library;
    std::uint32_t ordinal = 0;
    ImportKind kind = ImportKind::Function;
    ImportFunctionHandler function_handler = nullptr;
    void* function_context = nullptr;
    ImportVariableResolver variable_resolver = nullptr;
    void* variable_resolution_context = nullptr;
};

enum class ValidationError : std::uint8_t
{
    None,
    InvalidImageDigest,
    ImageSizeMismatch,
    ImageBaseMisaligned,
    ImageAddressOverflow,
    ImageDigestMismatch,
    InvalidCodeRange,
    EntryPointOutsideCode,
    ImportCountMismatch,
    InvalidImport,
    ImportOrdinalOutOfRange,
    ImportAddressConflict,
    UnsortedImportManifest,
    ImportManifestDigestMismatch,
    ImportBindingCountMismatch,
    ImportBindingMismatch,
    ImportBindingKindMismatch,
    ImportBindingCallbackMismatch,
    Count,
};

struct ValidationResult
{
    ValidationError error = ValidationError::None;
    std::string detail;

    [[nodiscard]] explicit operator bool() const noexcept { return error == ValidationError::None; }
};

[[nodiscard]] ValidationResult ValidateModule(const GuestModule& module);
[[nodiscard]] ValidationResult ValidateImports(const GuestModule& module,
                                               std::span<const ImportBinding> bindings);
[[nodiscard]] std::string_view ToString(ValidationError error) noexcept;

} // namespace x360port

#endif
