#include "x360port/runtime.hpp"

namespace x360port
{

std::string_view ToString(RuntimeError error) noexcept
{
    switch (error)
    {
    case RuntimeError::None:
        return "none";
    case RuntimeError::InstanceAlreadyActive:
        return "instance already active";
    case RuntimeError::MemoryInitializationFailed:
        return "memory initialization failed";
    case RuntimeError::BackendInitializationFailed:
        return "dynarec backend initialization failed";
    case RuntimeError::StackAllocationFailed:
        return "guest stack allocation failed";
    case RuntimeError::ModuleAlreadyLoaded:
        return "module already loaded";
    case RuntimeError::ModuleValidationFailed:
        return "module validation failed";
    case RuntimeError::ImportValidationFailed:
        return "import validation failed";
    case RuntimeError::VariableResolutionFailed:
        return "variable import resolution failed";
    case RuntimeError::ImportAttachmentFailed:
        return "Xenia import attachment failed";
    case RuntimeError::LoadStateInvalid:
        return "runtime load state invalid";
    case RuntimeError::ImageAllocationFailed:
        return "guest image allocation failed";
    case RuntimeError::ModuleRegistrationFailed:
        return "RawModule registration failed";
    case RuntimeError::EntryOutsideCode:
        return "entry outside code";
    case RuntimeError::TranslationFailed:
        return "translation failed";
    case RuntimeError::ExecutionBudgetInvalid:
        return "invalid guest execution block budget";
    case RuntimeError::ExecutionBudgetExceeded:
        return "guest execution block budget exceeded";
    case RuntimeError::ExecutionInvalidated:
        return "guest execution invalidated by executable write";
    case RuntimeError::ImportServiceRefused:
        return "host import service refused guest execution";
    case RuntimeError::ExecutionFailed:
        return "execution failed";
    case RuntimeError::OverrideInvalid:
        return "invalid native override";
    case RuntimeError::OverrideAlreadyInstalled:
        return "native override already installed";
    case RuntimeError::OverrideNotInstalled:
        return "native override not installed";
    case RuntimeError::DeviceRangeInvalid:
        return "invalid device-memory range";
    case RuntimeError::DeviceRangeRegistrationFailed:
        return "device-memory range registration failed";
    case RuntimeError::ExecutableRangeInvalid:
        return "invalid executable write range";
    case RuntimeError::ExecutableWatchRegistrationFailed:
        return "executable-write observation registration failed";
    case RuntimeError::ExecutableWatchRearmFailed:
        return "executable-write observation rearm failed";
    case RuntimeError::GuestMemoryAllocationFailed:
        return "guest memory allocation failed";
    case RuntimeError::GuestMemoryRangeInvalid:
        return "invalid guest memory range";
    }
    return "unknown runtime error";
}

} // namespace x360port
