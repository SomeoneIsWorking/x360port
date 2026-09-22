#include "x360port/runtime.hpp"

#include "device_dispatch.hpp"
#include "executable_invalidation.hpp"
#include "guest_execution.hpp"
#include "guest_fault_guard.hpp"
#include "guest_memory.hpp"
#include "guest_range_reservation.hpp"
#include "guest_thread_context.hpp"
#include "guest_virtual_memory.hpp"
#include "interpreter_fallback.hpp"
#include "override_dispatch.hpp"
#include "runtime_imports.hpp"
#include "xenia_backend.hpp"
#include "xenia_instance_lease.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

#include "xenia/cpu/function.h"
#include "xenia/cpu/processor.h"
#include "xenia/cpu/raw_module.h"
#include "xenia/memory.h"

namespace x360port
{
namespace
{

[[nodiscard]] RuntimeFailure Failure(RuntimeError error, std::string detail)
{
    return RuntimeFailure{error, std::move(detail)};
}

} // namespace

class RuntimeContext::Impl final
{
  public:
    Impl() = default;
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    ~Impl()
    {
        thread_.Reset();
        // The guard holds the processor's code cache and an installed handler,
        // so it is torn down before the processor it observes.
        fault_guard_.reset();
        processor_.reset();
        if (memory_ != nullptr && image_address_ != 0)
        {
            if (auto* image_heap = memory_->LookupHeap(image_address_); image_heap != nullptr)
            {
                static_cast<void>(image_heap->Release(image_address_));
            }
        }
        export_resolver_.reset();
        imports_.reset();
        guest_memory_.Reset();
        invalidation_.reset();
        memory_.reset();
    }

    [[nodiscard]] RuntimeFailure Initialize()
    {
        if (!instance_.Acquire())
        {
            return Failure(RuntimeError::InstanceAlreadyActive,
                           "Xenia guest memory uses a process-wide fixed mapping; only one "
                           "x360port runtime or system session may be active");
        }

        memory_ = std::make_unique<xe::Memory>();
        if (!memory_->Initialize())
        {
            return Failure(RuntimeError::MemoryInitializationFailed,
                           "Xenia Memory::Initialize refused its guest address-space mapping");
        }
        guest_memory_.Initialize(*memory_);

        export_resolver_ = std::make_unique<xe::cpu::ExportResolver>();
        processor_ = std::make_unique<xe::cpu::Processor>(memory_.get(), export_resolver_.get());
        if (!processor_->Setup(CreateXeniaHostBackend()))
        {
            return Failure(RuntimeError::BackendInitializationFailed,
                           "Xenia Processor::Setup refused the host dynarec backend");
        }

        // Installed after Processor::Setup so Xenia's own MMIO and write-watch
        // handlers stay ahead of it in the chain and keep the faults they own.
        fault_guard_ =
            std::make_unique<GuestFaultGuard>(*processor_->backend()->code_cache(), *memory_);

        if (RuntimeFailure thread_failure = thread_.Initialize(*memory_, *processor_);
            thread_failure)
        {
            return thread_failure;
        }
        virtual_memory_.emplace(*memory_);
        kernel_claims_ = virtual_memory_->Claims();
        processor_->PreLaunch();
        return {};
    }

    [[nodiscard]] RuntimeFailure LoadModule(const GuestModule& module,
                                            std::span<const ImportBinding> bindings)
    {
        if (load_failed_)
        {
            return Failure(RuntimeError::LoadStateInvalid,
                           "a previous import/module attachment failed; recreate the context");
        }
        if (module_ != nullptr)
        {
            return Failure(RuntimeError::ModuleAlreadyLoaded,
                           "a RuntimeContext owns exactly one authenticated guest image");
        }

        const ValidationResult module_validation = ValidateModule(module);
        if (!module_validation)
        {
            return Failure(RuntimeError::ModuleValidationFailed,
                           std::string(ToString(module_validation.error)) + ": " +
                               module_validation.detail);
        }
        const ValidationResult import_validation = ValidateImports(module, bindings);
        if (!import_validation)
        {
            return Failure(RuntimeError::ImportValidationFailed,
                           std::string(ToString(import_validation.error)) + ": " +
                               import_validation.detail);
        }
        RuntimeImportsCreateResult staged_imports =
            RuntimeImports::Create(module.ImportManifest(), bindings);
        if (!staged_imports)
        {
            return std::move(staged_imports.failure);
        }

        const ModuleDescriptor& descriptor = module.Descriptor();
        auto* heap = memory_->LookupHeap(descriptor.image.base);
        if (heap == nullptr ||
            !heap->AllocFixed(descriptor.image.base, descriptor.image.size, 0,
                              xe::kMemoryAllocationReserve | xe::kMemoryAllocationCommit,
                              xe::kMemoryProtectRead | xe::kMemoryProtectWrite))
        {
            return Failure(RuntimeError::ImageAllocationFailed,
                           "Xenia refused the authenticated image's exact guest range");
        }
        GuestRangeReservation image_reservation(*heap, descriptor.image.base);

        std::memcpy(memory_->TranslateVirtual(descriptor.image.base), module.ImageBytes().data(),
                    module.ImageBytes().size());

        auto raw_module = std::make_unique<xe::cpu::RawModule>(processor_.get());
        raw_module->set_name("x360port-authenticated-image");
        raw_module->set_executable(true);
        raw_module->SetAddressRange(descriptor.code.base, descriptor.code.size);
        imports_ = std::move(staged_imports.imports);
        RuntimeFailure import_failure;
        try
        {
            import_failure = imports_->Attach(*export_resolver_, *raw_module, *memory_);
        }
        catch (...)
        {
            load_failed_ = true;
            throw;
        }
        if (import_failure)
        {
            load_failed_ = true;
            return import_failure;
        }
        auto* registered_module = raw_module.get();
        try
        {
            if (!processor_->AddModule(std::move(raw_module)))
            {
                load_failed_ = true;
                return Failure(RuntimeError::ModuleRegistrationFailed,
                               "Xenia refused the authenticated RawModule registration");
            }
        }
        catch (...)
        {
            load_failed_ = true;
            throw;
        }
        module_ = registered_module;
        code_range_ = descriptor.code;
        image_address_ = descriptor.image.base;
        invalidation_ = std::make_unique<ExecutableInvalidation>(*processor_, statistics_);
        RuntimeFailure watch_failure = invalidation_->Arm(*memory_, code_range_);
        if (watch_failure)
        {
            load_failed_ = true;
            return watch_failure;
        }
        image_reservation.Commit();
        return {};
    }

    [[nodiscard]] RuntimeFailure InstallOverride(GuestAddress address,
                                                 NativeOverrideHandler handler, void* context)
    {
        if (module_ == nullptr)
        {
            return Failure(RuntimeError::LoadStateInvalid,
                           "native overrides require an authenticated image to be loaded");
        }
        return overrides_.Install(address, handler, context, code_range_);
    }

    [[nodiscard]] RuntimeFailure RemoveOverride(GuestAddress address)
    {
        return overrides_.Remove(address);
    }

    [[nodiscard]] RuntimeFailure RegisterDeviceMemoryRange(std::uint32_t address,
                                                           std::uint32_t mask, std::uint32_t size,
                                                           DeviceReadCallback read_callback,
                                                           DeviceWriteCallback write_callback,
                                                           void* context)
    {
        return devices_.Register(*memory_, address, mask, size, read_callback, write_callback,
                                 context);
    }

    [[nodiscard]] RuntimeFailure NotifyExecutableWrite(GuestAddress address, std::uint32_t size)
    {
        if (invalidation_ == nullptr)
        {
            return Failure(RuntimeError::LoadStateInvalid,
                           "executable writes require an authenticated image to be loaded");
        }
        return invalidation_->Notify(code_range_, address, size);
    }

    [[nodiscard]] ExecutionResult Execute(RuntimeContext& owner, GuestAddress address,
                                          std::span<const std::uint64_t> arguments,
                                          ExecutionLimits limits)
    {
        if (const auto override = overrides_.Find(address); override.has_value())
        {
            ++statistics_.native_override_calls;
            return override->handler(owner, address, arguments, override->context);
        }
        return ExecuteOriginal(address, arguments, limits);
    }

    [[nodiscard]] ExecutionResult CallOriginal(GuestAddress address,
                                               std::span<const std::uint64_t> arguments,
                                               ExecutionLimits limits)
    {
        ++statistics_.original_calls;
        return ExecuteOriginal(address, arguments, limits);
    }

    [[nodiscard]] ExecutionResult ExecuteOriginal(GuestAddress address,
                                                  std::span<const std::uint64_t> arguments,
                                                  ExecutionLimits limits)
    {
        if (invalidation_ != nullptr)
        {
            if (const RuntimeFailure failure = invalidation_->DrainObservedWrites())
            {
                return {failure, 0};
            }
        }
        if (!IsInCodeRange(address))
        {
            return {Failure(RuntimeError::EntryOutsideCode,
                            "guest entry is outside the authenticated executable range"),
                    0};
        }
        const auto* cached_guest_function =
            dynamic_cast<xe::cpu::GuestFunction*>(processor_->QueryFunction(address));
        const bool had_machine_code = cached_guest_function != nullptr &&
                                      cached_guest_function->machine_code() != nullptr &&
                                      cached_guest_function->machine_code_length() != 0;
        xe::cpu::Function* function = processor_->ResolveFunction(address);
        auto* guest_function = dynamic_cast<xe::cpu::GuestFunction*>(function);
        if (guest_function == nullptr || guest_function->machine_code() == nullptr ||
            guest_function->machine_code_length() == 0)
        {
            ++statistics_.translation_failures;
            return ExecuteInterpreterFallback(*processor_, thread_.State(), address, arguments,
                                              limits, statistics_);
        }

        if (!had_machine_code)
        {
            ++statistics_.translated_functions;
            statistics_.emitted_host_bytes += guest_function->machine_code_length();
        }

        ExecutionResult result =
            ExecuteGuestFunction(*function, thread_.State(), arguments, limits);
        if (result.failure.error == RuntimeError::ExecutionBudgetExceeded)
        {
            ++statistics_.execution_budget_exhaustions;
        }
        if (result.failure.error == RuntimeError::ExecutionInvalidated)
        {
            ++statistics_.execution_invalidations;
        }
        if (result.failure.error == RuntimeError::ImportServiceRefused)
        {
            ++statistics_.import_service_refusals;
        }
        if (result.failure.error == RuntimeError::GuestAccessViolation)
        {
            ++statistics_.guest_access_violations;
        }
        if (result)
        {
            ++statistics_.execution_calls;
        }
        return result;
    }

    [[nodiscard]] const JitStatistics& Statistics() const noexcept { return statistics_; }
    [[nodiscard]] GuestMemory& GuestMemoryOwner() noexcept { return guest_memory_; }
    [[nodiscard]] std::span<const ImportClaim> KernelServiceClaims() const noexcept
    {
        return kernel_claims_;
    }

    void BindOverrideOwner(RuntimeContext& owner) noexcept
    {
        overrides_.Bind(*processor_, owner, statistics_);
    }

  private:
    [[nodiscard]] bool IsInCodeRange(GuestAddress address) const noexcept
    {
        return module_ != nullptr && address >= code_range_.base &&
               static_cast<std::uint64_t>(address) <
                   static_cast<std::uint64_t>(code_range_.base) + code_range_.size;
    }

    // Declared first so it is released last, after every Xenia owner below.
    XeniaInstanceLease instance_;
    bool load_failed_ = false;
    std::unique_ptr<xe::Memory> memory_;
    GuestMemory guest_memory_;
    std::unique_ptr<xe::cpu::ExportResolver> export_resolver_;
    std::unique_ptr<xe::cpu::Processor> processor_;
    GuestThreadContext thread_;
    std::optional<GuestVirtualMemory> virtual_memory_;
    std::array<ImportClaim, 3> kernel_claims_{};
    std::unique_ptr<GuestFaultGuard> fault_guard_;
    std::unique_ptr<ExecutableInvalidation> invalidation_;
    std::unique_ptr<RuntimeImports> imports_;
    xe::cpu::RawModule* module_ = nullptr;
    CodeRange code_range_{};
    std::uint32_t image_address_ = 0;
    OverrideDispatch overrides_;
    JitStatistics statistics_{};
    DeviceDispatch devices_{statistics_};
};

RuntimeContext::RuntimeContext(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl))
{
    impl_->BindOverrideOwner(*this);
}

RuntimeContext::~RuntimeContext() = default;

RuntimeCreateResult RuntimeContext::Create()
{
    auto impl = std::make_unique<Impl>();
    RuntimeFailure failure = impl->Initialize();
    if (failure)
    {
        return {nullptr, std::move(failure)};
    }
    return {std::unique_ptr<RuntimeContext>(new RuntimeContext(std::move(impl))), {}};
}

GuestMemoryAllocationResult RuntimeContext::AllocateGuestMemory(std::uint32_t size)
{
    return impl_->GuestMemoryOwner().Allocate(size);
}
RuntimeFailure RuntimeContext::ReadGuestMemory(GuestAddress address,
                                               std::span<std::byte> bytes) const
{
    return impl_->GuestMemoryOwner().Read(address, bytes);
}
RuntimeFailure RuntimeContext::WriteGuestMemory(GuestAddress address,
                                                std::span<const std::byte> bytes)
{
    return impl_->GuestMemoryOwner().Write(address, bytes);
}
RuntimeFailure RuntimeContext::ReadMappedGuestMemory(GuestAddress address,
                                                     std::span<std::byte> bytes) const
{
    return impl_->GuestMemoryOwner().ReadMapped(address, bytes);
}
RuntimeFailure RuntimeContext::WriteMappedGuestMemory(GuestAddress address,
                                                      std::span<const std::byte> bytes)
{
    return impl_->GuestMemoryOwner().WriteMapped(address, bytes);
}
RuntimeFailure RuntimeContext::ReleaseGuestMemory(GuestMemoryAllocation allocation)
{
    return impl_->GuestMemoryOwner().Release(allocation);
}

std::span<const ImportClaim> RuntimeContext::KernelServiceClaims() const noexcept
{
    return impl_->KernelServiceClaims();
}

RuntimeFailure RuntimeContext::LoadModule(const GuestModule& module,
                                          std::span<const ImportBinding> bindings)
{
    return impl_->LoadModule(module, bindings);
}

RuntimeFailure RuntimeContext::InstallOverride(GuestAddress address, NativeOverrideHandler handler,
                                               void* context)
{
    return impl_->InstallOverride(address, handler, context);
}

RuntimeFailure RuntimeContext::RemoveOverride(GuestAddress address)
{
    return impl_->RemoveOverride(address);
}

RuntimeFailure RuntimeContext::RegisterDeviceMemoryRange(std::uint32_t address, std::uint32_t mask,
                                                         std::uint32_t size,
                                                         DeviceReadCallback read_callback,
                                                         DeviceWriteCallback write_callback,
                                                         void* context)
{
    return impl_->RegisterDeviceMemoryRange(address, mask, size, read_callback, write_callback,
                                            context);
}

RuntimeFailure RuntimeContext::NotifyExecutableWrite(GuestAddress address, std::uint32_t size)
{
    return impl_->NotifyExecutableWrite(address, size);
}

ExecutionResult RuntimeContext::Execute(GuestAddress address,
                                        std::span<const std::uint64_t> arguments,
                                        ExecutionLimits limits)
{
    return impl_->Execute(*this, address, arguments, limits);
}

ExecutionResult RuntimeContext::CallOriginal(GuestAddress address,
                                             std::span<const std::uint64_t> arguments,
                                             ExecutionLimits limits)
{
    return impl_->CallOriginal(address, arguments, limits);
}

ExecutionResult RuntimeContext::CallOriginalBody(GuestAddress address,
                                                 std::span<const std::uint64_t> arguments)
{
    return impl_->CallOriginal(address, arguments, ExecutionLimits{});
}

const JitStatistics& RuntimeContext::Statistics() const noexcept { return impl_->Statistics(); }

} // namespace x360port
