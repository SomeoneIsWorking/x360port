#include "x360port/runtime.hpp"

#include "device_dispatch.hpp"
#include "executable_invalidation.hpp"
#include "guest_call_frame.hpp"
#include "override_dispatch.hpp"
#include "runtime_imports.hpp"
#include "xenia_backend.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <utility>

#include "xenia/cpu/function.h"
#include "xenia/cpu/ppc/ppc_context.h"
#include "xenia/cpu/processor.h"
#include "xenia/cpu/raw_module.h"
#include "xenia/cpu/thread_state.h"
#include "xenia/memory.h"

namespace x360port
{
namespace
{

constexpr std::uint32_t kThreadId = 0x360;
constexpr std::uint32_t kStackSize = 64 * 1024;
constexpr std::uint32_t kReturnAddress = 0xBCBCBCBC;
constexpr std::size_t kRegisterArgumentCount = 8;

std::mutex g_instance_mutex;
bool g_instance_active = false;

[[nodiscard]] RuntimeFailure Failure(RuntimeError error, std::string detail)
{
    return RuntimeFailure{error, std::move(detail)};
}

class GuestRangeReservation final
{
  public:
    GuestRangeReservation(xe::BaseHeap& heap, std::uint32_t address) noexcept
        : heap_(&heap), address_(address)
    {
    }

    GuestRangeReservation(const GuestRangeReservation&) = delete;
    GuestRangeReservation& operator=(const GuestRangeReservation&) = delete;

    ~GuestRangeReservation()
    {
        if (heap_ != nullptr)
        {
            static_cast<void>(heap_->Release(address_));
        }
    }

    void Commit() noexcept { heap_ = nullptr; }

  private:
    xe::BaseHeap* heap_;
    std::uint32_t address_;
};

} // namespace

class RuntimeContext::Impl final
{
  public:
    Impl() = default;
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    ~Impl()
    {
        thread_state_.reset();
        if (memory_ != nullptr && stack_address_ != 0)
        {
            memory_->SystemHeapFree(stack_address_);
        }
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
        memory_.reset();

        if (owns_instance_)
        {
            const std::scoped_lock lock(g_instance_mutex);
            g_instance_active = false;
        }
    }

    [[nodiscard]] RuntimeFailure Initialize()
    {
        {
            const std::scoped_lock lock(g_instance_mutex);
            if (g_instance_active)
            {
                return Failure(RuntimeError::InstanceAlreadyActive,
                               "Xenia guest memory uses a process-wide fixed mapping; only one "
                               "x360port RuntimeContext may be active");
            }
            g_instance_active = true;
            owns_instance_ = true;
        }

        memory_ = std::make_unique<xe::Memory>();
        if (!memory_->Initialize())
        {
            return Failure(RuntimeError::MemoryInitializationFailed,
                           "Xenia Memory::Initialize refused its guest address-space mapping");
        }

        export_resolver_ = std::make_unique<xe::cpu::ExportResolver>();
        processor_ = std::make_unique<xe::cpu::Processor>(memory_.get(), export_resolver_.get());
        if (!processor_->Setup(CreateXeniaHostBackend()))
        {
            return Failure(RuntimeError::BackendInitializationFailed,
                           "Xenia Processor::Setup refused the host dynarec backend");
        }

        stack_address_ = memory_->SystemHeapAlloc(kStackSize);
        if (stack_address_ == 0)
        {
            return Failure(RuntimeError::StackAllocationFailed,
                           "Xenia could not allocate the bounded guest call stack");
        }
        thread_state_ = std::make_unique<xe::cpu::ThreadState>(processor_.get(), kThreadId,
                                                               stack_address_ + kStackSize);
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
        return overrides_.Install(address, handler, context, code_range_, InvalidateEntry, this);
    }

    [[nodiscard]] RuntimeFailure RemoveOverride(GuestAddress address)
    {
        return overrides_.Remove(address, InvalidateEntry, this);
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
                                          std::span<const std::uint64_t> arguments)
    {
        if (const auto override = overrides_.Find(address); override.has_value())
        {
            ++statistics_.native_override_calls;
            return override->handler(owner, address, arguments, override->context);
        }
        return ExecuteOriginal(address, arguments);
    }

    [[nodiscard]] ExecutionResult CallOriginal(GuestAddress address,
                                               std::span<const std::uint64_t> arguments)
    {
        ++statistics_.original_calls;
        return ExecuteOriginal(address, arguments);
    }

    [[nodiscard]] ExecutionResult ExecuteOriginal(GuestAddress address,
                                                  std::span<const std::uint64_t> arguments)
    {
        if (!IsInCodeRange(address))
        {
            return {Failure(RuntimeError::EntryOutsideCode,
                            "guest entry is outside the authenticated executable range"),
                    0};
        }
        if (arguments.size() > kRegisterArgumentCount)
        {
            return {Failure(RuntimeError::ExecutionFailed,
                            "this bounded call contract accepts at most eight register arguments"),
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
            return {Failure(RuntimeError::TranslationFailed,
                            "Xenia could not translate the requested guest function"),
                    0};
        }

        if (!had_machine_code)
        {
            ++statistics_.translated_functions;
            statistics_.emitted_host_bytes += guest_function->machine_code_length();
        }

        auto* context = thread_state_->context();
        for (std::size_t index = 0; index < arguments.size(); ++index)
        {
            context->r[3 + index] = arguments[index];
        }
        const GuestCallFrame call_frame(*context, kReturnAddress);
        const bool executed = function->Call(thread_state_.get(), kReturnAddress);
        if (!executed)
        {
            return {Failure(RuntimeError::ExecutionFailed,
                            "Xenia's translated guest function refused execution"),
                    0};
        }

        ++statistics_.execution_calls;
        return {{}, context->r[3]};
    }

    [[nodiscard]] const JitStatistics& Statistics() const noexcept { return statistics_; }

  private:
    static void InvalidateEntry(void* context, GuestAddress address) noexcept
    {
        auto& runtime = *static_cast<Impl*>(context);
        runtime.processor_->RemoveFunctionByAddress(address);
        ++runtime.statistics_.translation_invalidations;
    }

    [[nodiscard]] bool IsInCodeRange(GuestAddress address) const noexcept
    {
        return module_ != nullptr && address >= code_range_.base &&
               static_cast<std::uint64_t>(address) <
                   static_cast<std::uint64_t>(code_range_.base) + code_range_.size;
    }

    bool owns_instance_ = false;
    bool load_failed_ = false;
    std::unique_ptr<xe::Memory> memory_;
    std::unique_ptr<xe::cpu::ExportResolver> export_resolver_;
    std::unique_ptr<xe::cpu::Processor> processor_;
    std::unique_ptr<xe::cpu::ThreadState> thread_state_;
    std::unique_ptr<ExecutableInvalidation> invalidation_;
    std::unique_ptr<RuntimeImports> imports_;
    xe::cpu::RawModule* module_ = nullptr;
    CodeRange code_range_{};
    std::uint32_t stack_address_ = 0;
    std::uint32_t image_address_ = 0;
    OverrideDispatch overrides_;
    JitStatistics statistics_{};
    DeviceDispatch devices_{statistics_};
};

RuntimeContext::RuntimeContext(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}

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
                                        std::span<const std::uint64_t> arguments)
{
    return impl_->Execute(*this, address, arguments);
}

ExecutionResult RuntimeContext::CallOriginal(GuestAddress address,
                                             std::span<const std::uint64_t> arguments)
{
    return impl_->CallOriginal(address, arguments);
}

const JitStatistics& RuntimeContext::Statistics() const noexcept { return impl_->Statistics(); }

} // namespace x360port
