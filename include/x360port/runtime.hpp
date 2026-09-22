#ifndef X360PORT_RUNTIME_HPP
#define X360PORT_RUNTIME_HPP

#include "x360port/guest_call.hpp"
#include "x360port/import_claims.hpp"
#include "x360port/module_contract.hpp"
#include "x360port/runtime_failure.hpp"
#include "x360port/validation.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace x360port
{

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

using DeviceReadCallback = std::uint32_t (*)(std::uint32_t address, void* context) noexcept;
using DeviceWriteCallback = void (*)(std::uint32_t address, std::uint32_t value,
                                     void* context) noexcept;

struct JitStatistics
{
    std::uint64_t translated_functions = 0;
    std::uint64_t emitted_host_bytes = 0;
    std::uint64_t translation_failures = 0;
    std::uint64_t execution_calls = 0;
    std::uint64_t execution_budget_exhaustions = 0;
    std::uint64_t execution_invalidations = 0;
    std::uint64_t import_service_refusals = 0;
    std::uint64_t native_override_calls = 0;
    std::uint64_t original_calls = 0;
    std::uint64_t translation_invalidations = 0;
    std::uint64_t observed_executable_writes = 0;
    std::uint64_t device_read_calls = 0;
    std::uint64_t device_write_calls = 0;
    std::uint64_t interpreter_fallback_entries = 0;
    std::uint64_t interpreter_fallback_instructions = 0;
    std::uint64_t interpreter_fallback_unsupported = 0;
    std::uint64_t interpreter_fallback_memory_failures = 0;
    std::uint64_t interpreter_fallback_budget_exhaustions = 0;
    std::uint64_t guest_access_violations = 0;
};

struct ExecutionLimits
{
    // Counts translated guest basic-block entries across nested guest calls.
    // A zero limit is invalid; the default is intentionally finite so a guest
    // path that never returns cannot strand the embedding thread. The budget
    // is decremented at block entry, so it cannot bound an instruction that
    // faults repeatedly; GuestFaultGuard terminates that call instead.
    std::uint64_t max_guest_blocks = 1'000'000;
    // Bounds the fallback when JIT compilation refuses.
    std::uint64_t max_interpreter_instructions = 100'000;
};

struct RuntimeCreateResult;

class RuntimeContext final : public GuestCallContext
{
  public:
    RuntimeContext(const RuntimeContext&) = delete;
    RuntimeContext& operator=(const RuntimeContext&) = delete;
    RuntimeContext(RuntimeContext&&) = delete;
    RuntimeContext& operator=(RuntimeContext&&) = delete;
    ~RuntimeContext();

    [[nodiscard]] static RuntimeCreateResult Create();

    [[nodiscard]] GuestMemoryAllocationResult AllocateGuestMemory(std::uint32_t size);
    [[nodiscard]] RuntimeFailure ReadGuestMemory(GuestAddress address,
                                                 std::span<std::byte> bytes) const;
    [[nodiscard]] RuntimeFailure WriteGuestMemory(GuestAddress address,
                                                  std::span<const std::byte> bytes);
    // Reads and writes title-owned mapped memory. Unlike the allocation API,
    // these operations do not require x360port to own the allocation, but
    // they still require one committed, accessible guest range and reject
    // device mappings.
    [[nodiscard]] RuntimeFailure ReadMappedGuestMemory(GuestAddress address,
                                                       std::span<std::byte> bytes) const override;
    [[nodiscard]] RuntimeFailure WriteMappedGuestMemory(GuestAddress address,
                                                        std::span<const std::byte> bytes) override;
    [[nodiscard]] RuntimeFailure ReleaseGuestMemory(GuestMemoryAllocation allocation);

    // The kernel exports x360port implements over its own embedded Xenia
    // state. A consumer resolves these beside its title services so one table
    // decides which import each ordinal reaches; an export absent here keeps
    // the consumer's own refusal.
    [[nodiscard]] std::span<const ImportClaim> KernelServiceClaims() const noexcept;

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
                                          std::span<const std::uint64_t> arguments = {},
                                          ExecutionLimits limits = {});
    [[nodiscard]] ExecutionResult CallOriginal(GuestAddress address,
                                               std::span<const std::uint64_t> arguments = {},
                                               ExecutionLimits limits = {});
    // The override-side original call: the same scoped entry with default limits.
    [[nodiscard]] ExecutionResult
    CallOriginalBody(GuestAddress address, std::span<const std::uint64_t> arguments) override;

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

} // namespace x360port

#endif
