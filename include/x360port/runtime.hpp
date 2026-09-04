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
};

struct RuntimeFailure
{
    RuntimeError error = RuntimeError::None;
    std::string detail;

    [[nodiscard]] explicit operator bool() const noexcept { return error != RuntimeError::None; }
};

struct JitStatistics
{
    std::uint64_t translated_functions = 0;
    std::uint64_t emitted_host_bytes = 0;
    std::uint64_t execution_calls = 0;
};

struct ExecutionResult
{
    RuntimeFailure failure;
    std::uint64_t value = 0;

    [[nodiscard]] explicit operator bool() const noexcept { return !failure; }
};

class RuntimeContext;
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

    [[nodiscard]] RuntimeFailure LoadModule(const GuestModule& module,
                                            std::span<const ImportBinding> bindings);
    [[nodiscard]] ExecutionResult Execute(GuestAddress address,
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
