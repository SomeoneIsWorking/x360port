#ifndef X360PORT_VALIDATION_HPP
#define X360PORT_VALIDATION_HPP

#include "x360port/module_contract.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace x360port
{

class RuntimeImports;

class GuestImportContext final
{
  public:
    static constexpr std::size_t argument_count = 8;

    [[nodiscard]] std::uint64_t argument(std::size_t index) const noexcept
    {
        return index < arguments_.size() ? arguments_[index] : 0;
    }

    [[nodiscard]] bool read_memory(GuestAddress address,
                                   std::span<std::byte> destination) const noexcept;
    [[nodiscard]] bool write_memory(GuestAddress address,
                                    std::span<const std::byte> source) const noexcept;
    void set_return_value(std::uint64_t value) noexcept { return_value_ = value; }

  private:
    friend class RuntimeImports;

    explicit GuestImportContext(const std::array<std::uint64_t, argument_count>& arguments,
                                void* memory) noexcept
        : arguments_(arguments), return_value_(arguments[0]), memory_(memory)
    {
    }

    [[nodiscard]] std::uint64_t return_value() const noexcept { return return_value_; }

    std::array<std::uint64_t, argument_count> arguments_{};
    std::uint64_t return_value_ = 0;
    void* memory_ = nullptr;
};

using ImportFunctionHandler = void (*)(GuestImportContext& context,
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
