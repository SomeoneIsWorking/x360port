#ifndef X360PORT_RUNTIME_HPP
#define X360PORT_RUNTIME_HPP

#include "x360port/module_contract.hpp"
#include "x360port/validation.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
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
    ExecutionFailed,
    OverrideInvalid,
    OverrideAlreadyInstalled,
    OverrideNotInstalled,
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

struct GuestMemoryAllocation
{
    GuestAddress address = 0;
    std::uint32_t size = 0;

    [[nodiscard]] explicit operator bool() const noexcept { return address != 0 && size != 0; }
};

struct GuestMemoryAllocationResult
{
    GuestMemoryAllocation allocation;
    RuntimeFailure failure;

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return static_cast<bool>(allocation) && !failure;
    }
};

class RuntimeContext;

struct ExecutionResult
{
    RuntimeFailure failure;
    std::uint64_t value = 0;

    [[nodiscard]] explicit operator bool() const noexcept { return !failure; }
};

using NativeOverrideHandler = ExecutionResult (*)(RuntimeContext& runtime, GuestAddress address,
                                                  std::span<const std::uint64_t> arguments,
                                                  void* context) noexcept;

using DeviceReadCallback = std::uint32_t (*)(std::uint32_t address, void* context) noexcept;
using DeviceWriteCallback = void (*)(std::uint32_t address, std::uint32_t value,
                                     void* context) noexcept;

struct JitStatistics
{
    std::uint64_t translated_functions = 0;
    std::uint64_t emitted_host_bytes = 0;
    std::uint64_t execution_calls = 0;
    std::uint64_t native_override_calls = 0;
    std::uint64_t original_calls = 0;
    std::uint64_t translation_invalidations = 0;
    std::uint64_t observed_executable_writes = 0;
    std::uint64_t device_read_calls = 0;
    std::uint64_t device_write_calls = 0;
};

struct RuntimeCreateResult;

class RuntimeContext final
{
  public:
    RuntimeContext(const RuntimeContext&) = delete;
    RuntimeContext& operator=(const RuntimeContext&) = delete;
    RuntimeContext(RuntimeContext&&) = delete;
    RuntimeContext& operator=(RuntimeContext&&) = delete;
    ~RuntimeContext();

    [[nodiscard]] static RuntimeCreateResult Create();

    [[nodiscard]] GuestMemoryAllocationResult AllocateGuestMemory(std::uint32_t size);
    [[nodiscard]] RuntimeFailure WriteGuestMemory(GuestAddress address,
                                                  std::span<const std::byte> bytes);
    [[nodiscard]] RuntimeFailure ReleaseGuestMemory(GuestMemoryAllocation allocation);

    [[nodiscard]] RuntimeFailure LoadModule(const GuestModule& module,
                                            std::span<const ImportBinding> bindings);
    // Installs a title-owned native implementation at an image address. The
    // handler may call CallOriginal to re-enter Xenia for one scoped original
    // invocation; Execute never routes that call back through the override.
    [[nodiscard]] RuntimeFailure
    InstallOverride(GuestAddress address, NativeOverrideHandler handler, void* context = nullptr);
    [[nodiscard]] RuntimeFailure RemoveOverride(GuestAddress address);
    [[nodiscard]] RuntimeFailure RegisterDeviceMemoryRange(std::uint32_t address,
                                                           std::uint32_t mask, std::uint32_t size,
                                                           DeviceReadCallback read_callback,
                                                           DeviceWriteCallback write_callback,
                                                           void* context = nullptr);
    [[nodiscard]] RuntimeFailure NotifyExecutableWrite(GuestAddress address, std::uint32_t size);
    [[nodiscard]] ExecutionResult Execute(GuestAddress address,
                                          std::span<const std::uint64_t> arguments = {});
    [[nodiscard]] ExecutionResult CallOriginal(GuestAddress address,
                                               std::span<const std::uint64_t> arguments = {});

    [[nodiscard]] const JitStatistics& Statistics() const noexcept;

  private:
    class Impl;

    explicit RuntimeContext(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> impl_;
};

struct RuntimeCreateResult
{
    std::unique_ptr<RuntimeContext> context;
    RuntimeFailure failure;

    [[nodiscard]] explicit operator bool() const noexcept { return context != nullptr && !failure; }
};

[[nodiscard]] std::string_view ToString(RuntimeError error) noexcept;

} // namespace x360port

#endif
