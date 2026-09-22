#ifndef X360PORT_RUNTIME_FAILURE_HPP
#define X360PORT_RUNTIME_FAILURE_HPP

#include <cstdint>
#include <string>
#include <string_view>

namespace x360port
{

enum class RuntimeError : std::uint8_t
{
    None,
    InstanceAlreadyActive,
    MemoryInitializationFailed,
    BackendInitializationFailed,
    StackAllocationFailed,
    ModuleAlreadyLoaded,
    ModuleValidationFailed,
    ImportValidationFailed,
    VariableResolutionFailed,
    ImportAttachmentFailed,
    LoadStateInvalid,
    ImageAllocationFailed,
    ModuleRegistrationFailed,
    EntryOutsideCode,
    TranslationFailed,
    InterpreterFallbackUnsupported,
    InterpreterFallbackMemoryInvalid,
    InterpreterFallbackBudgetExceeded,
    ExecutionBudgetInvalid,
    ExecutionBudgetExceeded,
    ExecutionInvalidated,
    GuestAccessViolation,
    ImportServiceRefused,
    ExecutionFailed,
    OverrideInvalid,
    OverrideAlreadyInstalled,
    OverrideNotInstalled,
    OverrideDispatchFailed,
    DeviceRangeInvalid,
    DeviceRangeRegistrationFailed,
    ExecutableRangeInvalid,
    ExecutableWatchRegistrationFailed,
    ExecutableWatchRearmFailed,
    GuestMemoryAllocationFailed,
    GuestMemoryRangeInvalid,
};

struct RuntimeFailure
{
    RuntimeError error = RuntimeError::None;
    std::string detail;

    [[nodiscard]] explicit operator bool() const noexcept { return error != RuntimeError::None; }
};

[[nodiscard]] std::string_view ToString(RuntimeError error) noexcept;

} // namespace x360port

#endif // X360PORT_RUNTIME_FAILURE_HPP
