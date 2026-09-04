#include "x360port/runtime.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <utility>

#include "xenia/base/platform.h"
#include "xenia/cpu/backend/backend.h"
#if XE_ARCH_AMD64
#include "xenia/cpu/backend/x64/x64_backend.h"
#elif XE_ARCH_ARM64
#include "xenia/cpu/backend/a64/a64_backend.h"
#else
#error "x360port requires a Xenia x64 or A64 dynarec backend"
#endif
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

[[nodiscard]] std::unique_ptr<xe::cpu::backend::Backend> CreateHostBackend()
{
#if XE_ARCH_AMD64
    return std::make_unique<xe::cpu::backend::x64::X64Backend>();
#elif XE_ARCH_ARM64
    return std::make_unique<xe::cpu::backend::a64::A64Backend>();
#endif
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

class GuestCallFrame final
{
  public:
    explicit GuestCallFrame(xe::cpu::ppc::PPCContext& context) noexcept
        : context_(context), previous_stack_pointer_(context.r[1]),
          previous_link_register_(context.lr)
    {
        context_.r[1] -= 64 + 112;
        context_.lr = kReturnAddress;
    }

    GuestCallFrame(const GuestCallFrame&) = delete;
    GuestCallFrame& operator=(const GuestCallFrame&) = delete;

    ~GuestCallFrame()
    {
        context_.lr = previous_link_register_;
        context_.r[1] = previous_stack_pointer_;
    }

  private:
    xe::cpu::ppc::PPCContext& context_;
    std::uint64_t previous_stack_pointer_;
    std::uint64_t previous_link_register_;
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
        if (!processor_->Setup(CreateHostBackend()))
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
        if (!bindings.empty())
        {
            return Failure(RuntimeError::RuntimeImportsNotImplemented,
                           "typed imports validate but are not yet attached to Xenia exports");
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
        auto* registered_module = raw_module.get();
        if (!processor_->AddModule(std::move(raw_module)))
        {
            return Failure(RuntimeError::ModuleRegistrationFailed,
                           "Xenia refused the authenticated RawModule registration");
        }
        module_ = registered_module;
        code_range_ = descriptor.code;
        image_address_ = descriptor.image.base;
        image_reservation.Commit();
        return {};
    }

    [[nodiscard]] ExecutionResult Execute(GuestAddress address,
                                          std::span<const std::uint64_t> arguments)
    {
        if (module_ == nullptr || address < code_range_.base ||
            static_cast<std::uint64_t>(address) >=
                static_cast<std::uint64_t>(code_range_.base) + code_range_.size)
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
        const GuestCallFrame call_frame(*context);
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
    bool owns_instance_ = false;
    std::unique_ptr<xe::Memory> memory_;
    std::unique_ptr<xe::cpu::ExportResolver> export_resolver_;
    std::unique_ptr<xe::cpu::Processor> processor_;
    std::unique_ptr<xe::cpu::ThreadState> thread_state_;
    xe::cpu::RawModule* module_ = nullptr;
    CodeRange code_range_{};
    std::uint32_t stack_address_ = 0;
    std::uint32_t image_address_ = 0;
    JitStatistics statistics_{};
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

ExecutionResult RuntimeContext::Execute(GuestAddress address,
                                        std::span<const std::uint64_t> arguments)
{
    return impl_->Execute(address, arguments);
}

const JitStatistics& RuntimeContext::Statistics() const noexcept { return impl_->Statistics(); }

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
    case RuntimeError::RuntimeImportsNotImplemented:
        return "runtime imports not implemented";
    case RuntimeError::ImageAllocationFailed:
        return "guest image allocation failed";
    case RuntimeError::ModuleRegistrationFailed:
        return "RawModule registration failed";
    case RuntimeError::EntryOutsideCode:
        return "entry outside code";
    case RuntimeError::TranslationFailed:
        return "translation failed";
    case RuntimeError::ExecutionFailed:
        return "execution failed";
    }
    return "unknown runtime error";
}

} // namespace x360port
