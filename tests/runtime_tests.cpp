#include "x360port/runtime.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

namespace
{

using namespace x360port;

constexpr GuestAddress kCodeAddress = 0x82000000;
constexpr GuestAddress kSelfModifyingAddress = kCodeAddress + 48;
constexpr GuestAddress kInternalCallerAddress = kCodeAddress + 80;
constexpr GuestAddress kNonReturningAddress = kCodeAddress + 108;
constexpr GuestAddress kNonReturningCallerAddress = kCodeAddress + 112;
constexpr GuestAddress kMidCallInvalidationAddress = kCodeAddress + 128;
constexpr GuestAddress kDeviceReadAddress = kCodeAddress + 8;
constexpr GuestAddress kDeviceWriteAddress = kCodeAddress + 24;
constexpr std::uint32_t kDeviceAddress = 0xC0001000;
constexpr GuestAddress kImportedCallAddress = 0x83000000;
constexpr GuestAddress kFunctionImportAddress = kImportedCallAddress + 0x20;
constexpr GuestAddress kVariableCallAddress = kImportedCallAddress + 0x30;
constexpr GuestAddress kVariableRecordAddress = kImportedCallAddress + 0x48;
constexpr GuestAddress kFunctionRecordAddress = kImportedCallAddress + 0x4C;
constexpr GuestAddress kVariableValueAddress = kImportedCallAddress + 0x58;
class TestModule final : public GuestModule
{
  public:
    TestModule()
    {
        CopyWords(0, {0x3860002A, 0x4E800020}); // li r3, 42; blr
        CopyWords(8, {0x3C60C000, 0x60631000, 0x80630000, 0x4E800020});
        CopyWords(24, {0x3C60C000, 0x60631000, 0x38800063, 0x90830004, 0x38600007, 0x4E800020});
        CopyWords(48, {0x3C608200, 0x60630000, 0x3C803860, 0x6084002B, 0x90830000, 0x38600009,
                       0x4E800020});
        CopyWords(80, {0x7C0802A6, 0x4800000D, 0x7C0803A6, 0x4E800020, 0x38600011, 0x4E800020});
        CopyWords(108, {0x48000000}); // b .
        CopyWords(112, {0x48000009}); // bl +8, to the nested leaf
        CopyWords(120, {0x48000000}); // b .
        CopyWords(128, {0x3C608200, 0x60630098, 0x3C804E80, 0x60840020, 0x90830000, 0x7C6903A6,
                        0x4E800420});
        descriptor_.image.sha256 = HashBytes(image_);
        descriptor_.image.base = kCodeAddress;
        descriptor_.image.size = static_cast<std::uint32_t>(image_.size());
        descriptor_.image.entry_point = kCodeAddress;
        descriptor_.code = {kCodeAddress, static_cast<std::uint32_t>(image_.size())};
        descriptor_.import_count = 0;
        descriptor_.import_manifest_sha256 = HashImportManifest({});
    }

    [[nodiscard]] const ModuleDescriptor& Descriptor() const noexcept override
    {
        return descriptor_;
    }

    [[nodiscard]] std::span<const std::byte> ImageBytes() const noexcept override { return image_; }

    [[nodiscard]] std::span<const ImportRequirement> ImportManifest() const noexcept override
    {
        return {};
    }

  private:
    void CopyWords(std::size_t offset, std::initializer_list<std::uint32_t> words)
    {
        for (const std::uint32_t word : words)
        {
            image_[offset++] = static_cast<std::byte>(word >> 24);
            image_[offset++] = static_cast<std::byte>(word >> 16);
            image_[offset++] = static_cast<std::byte>(word >> 8);
            image_[offset++] = static_cast<std::byte>(word);
        }
    }

    std::array<std::byte, 156> image_{};
    ModuleDescriptor descriptor_{};
};

struct ImportObservations
{
    std::uint32_t function_calls = 0;
    std::uint32_t variable_resolutions = 0;
};

struct OverrideObservations
{
    std::uint32_t calls = 0;
};

struct DeviceObservations
{
    std::uint32_t reads = 0;
    std::uint32_t writes = 0;
    std::uint32_t last_write_address = 0;
    std::uint32_t last_write_value = 0;
};

std::uint32_t DeviceRead(std::uint32_t, void* context) noexcept
{
    ++static_cast<DeviceObservations*>(context)->reads;
    return 0x12345678;
}

constexpr auto DeviceWrite = [](const auto address, const auto value, void* context) noexcept
{
    auto& observations = *static_cast<DeviceObservations*>(context);
    ++observations.writes;
    observations.last_write_address = address;
    observations.last_write_value = value;
};

ExecutionResult AddOneThroughOriginal(RuntimeContext& runtime, GuestAddress address,
                                      std::span<const std::uint64_t> arguments,
                                      void* context) noexcept
{
    ++static_cast<OverrideObservations*>(context)->calls;
    ExecutionResult original = runtime.CallOriginal(address, arguments);
    if (original)
    {
        ++original.value;
    }
    return original;
}

void FunctionImport(void*, void*, void* context) noexcept
{
    ++static_cast<ImportObservations*>(context)->function_calls;
}

[[nodiscard]] GuestAddress ResolveVariable(void* context) noexcept
{
    auto& observations = *static_cast<ImportObservations*>(context);
    ++observations.variable_resolutions;
    return kVariableValueAddress;
}

[[nodiscard]] GuestAddress RefuseVariable(void* context) noexcept
{
    auto& observations = *static_cast<ImportObservations*>(context);
    ++observations.variable_resolutions;
    return 0;
}

class ImportedTestModule final : public GuestModule
{
  public:
    ImportedTestModule()
    {
        imports_[0] = {ImportKind::Function, "synthetic.xex",        1U,
                       "CallSynthetic",      kFunctionImportAddress, kFunctionRecordAddress};
        imports_[1] = {ImportKind::Variable, "synthetic.xex",        2U,
                       "VariableSynthetic",  kVariableRecordAddress, kVariableRecordAddress};
        constexpr std::array<std::uint8_t, 24> FunctionCaller{
            0x7D, 0x88, 0x02, 0xA6, // mflr r12
            0x38, 0x60, 0x00, 0x07, // li r3, 7
            0x48, 0x00, 0x00, 0x19, // bl +0x18
            0x38, 0x63, 0x00, 0x01, // addi r3, r3, 1
            0x7D, 0x88, 0x03, 0xA6, // mtlr r12
            0x4E, 0x80, 0x00, 0x20, // blr
        };
        constexpr std::array<std::uint8_t, 20> VariableCaller{
            0x3C, 0x80, 0x83, 0x00, // lis r4, 0x8300
            0x60, 0x84, 0x00, 0x48, // ori r4, r4, 0x48
            0x80, 0x84, 0x00, 0x00, // lwz r4, 0(r4)
            0x80, 0x64, 0x00, 0x00, // lwz r3, 0(r4)
            0x4E, 0x80, 0x00, 0x20, // blr
        };
        CopyBytes(0, FunctionCaller);
        CopyBytes(kVariableCallAddress - kImportedCallAddress, VariableCaller);
        constexpr std::array<std::uint8_t, 4> VariableValue{0x12, 0x34, 0x56, 0x78};
        CopyBytes(kVariableValueAddress - kImportedCallAddress, VariableValue);

        descriptor_.image.sha256 = HashBytes(image_);
        descriptor_.image.base = kImportedCallAddress;
        descriptor_.image.size = static_cast<std::uint32_t>(image_.size());
        descriptor_.image.entry_point = kImportedCallAddress;
        descriptor_.code = {kImportedCallAddress, 0x44U};
        descriptor_.import_count = imports_.size();
        descriptor_.import_manifest_sha256 = HashImportManifest(imports_);
    }

    [[nodiscard]] const ModuleDescriptor& Descriptor() const noexcept override
    {
        return descriptor_;
    }

    [[nodiscard]] std::span<const std::byte> ImageBytes() const noexcept override { return image_; }

    [[nodiscard]] std::span<const ImportRequirement> ImportManifest() const noexcept override
    {
        return imports_;
    }

  private:
    template <std::size_t Size>
    void CopyBytes(std::size_t offset, const std::array<std::uint8_t, Size>& bytes)
    {
        for (std::size_t index = 0; index < bytes.size(); ++index)
        {
            image_[offset + index] = static_cast<std::byte>(bytes[index]);
        }
    }

    std::array<std::byte, 0x60> image_{};
    std::array<ImportRequirement, 2> imports_;
    ModuleDescriptor descriptor_{};
};

[[noreturn]] void Fail(std::string_view message)
{
    std::cerr << "runtime contract test failed: " << message << '\n';
    std::exit(1);
}

void Require(bool condition, std::string_view message)
{
    if (!condition)
    {
        Fail(message);
    }
}

} // namespace

int main()
{
    RuntimeCreateResult created = RuntimeContext::Create();
    Require(static_cast<bool>(created), created.failure.detail);

    RuntimeCreateResult concurrent = RuntimeContext::Create();
    Require(!concurrent, "a second process-global Xenia mapping was accepted");
    Require(concurrent.failure.error == RuntimeError::InstanceAlreadyActive,
            "the concurrent-context refusal was not typed");

    GuestMemoryAllocationResult guest_allocation = created.context->AllocateGuestMemory(0x20U);
    Require(static_cast<bool>(guest_allocation), guest_allocation.failure.detail);
    const std::array<std::byte, 4> guest_bytes{std::byte{0x12}, std::byte{0x34}, std::byte{0x56},
                                               std::byte{0x78}};
    RuntimeFailure guest_write =
        created.context->WriteGuestMemory(guest_allocation.allocation.address + 4U, guest_bytes);
    Require(!guest_write, guest_write.detail);
    guest_write =
        created.context->WriteGuestMemory(guest_allocation.allocation.address + 0x1FU, guest_bytes);
    Require(guest_write.error == RuntimeError::GuestMemoryRangeInvalid,
            "a guest-memory write outside its allocation was accepted");
    guest_write = created.context->ReleaseGuestMemory(guest_allocation.allocation);
    Require(!guest_write, guest_write.detail);

    TestModule module;
    RuntimeFailure loaded = created.context->LoadModule(module, {});
    Require(!loaded, loaded.detail);

    ExecutionResult first = created.context->Execute(kCodeAddress);
    Require(static_cast<bool>(first), first.failure.detail);
    Require(first.value == 42, "the translated PPC leaf returned the wrong value");
    Require(created.context->Statistics().translated_functions == 1,
            "the cold call did not report one translated guest function");
    Require(created.context->Statistics().emitted_host_bytes > 0,
            "the cold call did not report emitted host code");
    Require(created.context->Statistics().execution_calls == 1,
            "the first translated call was not counted");

    ExecutionResult second = created.context->Execute(kCodeAddress);
    Require(static_cast<bool>(second), second.failure.detail);
    Require(second.value == 42, "the cached PPC leaf returned the wrong value");
    Require(created.context->Statistics().translated_functions == 1,
            "the cache hit was incorrectly reported as another translation");
    Require(created.context->Statistics().execution_calls == 2,
            "the cache-hit call was not counted");

    const std::uint64_t translations_before_internal_call =
        created.context->Statistics().translated_functions;
    ExecutionResult internal_call = created.context->Execute(kInternalCallerAddress);
    Require(static_cast<bool>(internal_call), internal_call.failure.detail);
    Require(internal_call.value == 17, "the internal guest call returned the wrong value");
    Require(created.context->Statistics().translated_functions > translations_before_internal_call,
            "the internal guest call did not translate through Xenia");
    const std::uint64_t translations_after_internal_call =
        created.context->Statistics().translated_functions;
    internal_call = created.context->Execute(kInternalCallerAddress);
    Require(static_cast<bool>(internal_call), internal_call.failure.detail);
    Require(internal_call.value == 17, "the cached internal guest call returned the wrong value");
    Require(created.context->Statistics().translated_functions == translations_after_internal_call,
            "the cached internal guest call translated again");

    ExecutionResult invalid_budget = created.context->Execute(kCodeAddress, {}, ExecutionLimits{0});
    Require(!invalid_budget, "a zero guest execution budget was accepted");
    Require(invalid_budget.failure.error == RuntimeError::ExecutionBudgetInvalid,
            "the invalid guest execution budget did not report its typed reason");

    ExecutionResult non_returning =
        created.context->Execute(kNonReturningAddress, {}, ExecutionLimits{2});
    Require(!non_returning, "a non-returning guest block escaped its execution budget");
    Require(non_returning.failure.error == RuntimeError::ExecutionBudgetExceeded,
            "the bounded guest exit did not report its typed reason");
    Require(created.context->Statistics().execution_budget_exhaustions == 1,
            "the bounded guest exit was not counted");
    non_returning = created.context->Execute(kNonReturningCallerAddress, {}, ExecutionLimits{2});
    Require(!non_returning, "a nested non-returning guest call escaped its budget");
    Require(non_returning.failure.error == RuntimeError::ExecutionBudgetExceeded,
            "nested bounded guest exit did not propagate its typed reason");
    Require(created.context->Statistics().execution_budget_exhaustions == 2,
            "the nested bounded guest exit was not counted");

    ExecutionResult mid_call_invalidation = created.context->Execute(kMidCallInvalidationAddress);
    Require(!mid_call_invalidation, "a self-modifying guest call continued after its write");
    Require(mid_call_invalidation.failure.error == RuntimeError::ExecutionInvalidated,
            "the mid-call executable write did not report its typed exit reason");
    Require(created.context->Statistics().execution_invalidations == 1,
            "the mid-call executable write was not counted");
    ExecutionResult after_mid_call = created.context->Execute(kCodeAddress);
    Require(static_cast<bool>(after_mid_call), after_mid_call.failure.detail);
    Require(created.context->Statistics().translation_invalidations >= 1,
            "the mid-call executable write was not drained before the next guest call");

    ExecutionResult self_modifying = created.context->Execute(kSelfModifyingAddress);
    Require(!self_modifying, "a self-modifying guest call continued after its executable write");
    Require(self_modifying.failure.error == RuntimeError::ExecutionInvalidated,
            "the self-modifying guest write did not report its typed exit reason");
    Require(created.context->Statistics().execution_invalidations == 2,
            "the self-modifying guest write was not counted");
    Require(created.context->Statistics().observed_executable_writes != 0,
            "Xenia did not observe the guest executable write");
    ExecutionResult automatically_invalidated = created.context->Execute(kCodeAddress);
    Require(static_cast<bool>(automatically_invalidated), automatically_invalidated.failure.detail);
    Require(automatically_invalidated.value == 43,
            "automatic executable invalidation did not expose the modified guest code");

    ExecutionResult outside = created.context->Execute(kCodeAddress + 0x1000);
    Require(!outside, "an entry outside authenticated code executed");
    Require(outside.failure.error == RuntimeError::EntryOutsideCode,
            "the out-of-range execution refusal was not typed");

    created.context.reset();
    RuntimeCreateResult recreated = RuntimeContext::Create();
    Require(static_cast<bool>(recreated), recreated.failure.detail);
    loaded = recreated.context->LoadModule(module, {});
    Require(!loaded, "a released RuntimeContext left its guest image range committed");
    ExecutionResult after_recreate = recreated.context->Execute(kCodeAddress);
    Require(static_cast<bool>(after_recreate), after_recreate.failure.detail);
    Require(after_recreate.value == 42, "the recreated runtime returned the wrong value");

    OverrideObservations override_observations;
    loaded = recreated.context->InstallOverride(kCodeAddress, AddOneThroughOriginal,
                                                &override_observations);
    Require(!loaded, loaded.detail);
    ExecutionResult overridden = recreated.context->Execute(kCodeAddress);
    Require(static_cast<bool>(overridden), overridden.failure.detail);
    Require(overridden.value == 43, "the native override did not wrap the original guest call");
    Require(override_observations.calls == 1,
            "the native override handler was not called exactly once");
    Require(recreated.context->Statistics().native_override_calls == 1,
            "the native override dispatch was not counted");
    Require(recreated.context->Statistics().original_calls == 1,
            "the scoped original call was not counted");
    Require(recreated.context->Statistics().translation_invalidations == 1,
            "installing the native override did not invalidate the guest entry");

    loaded = recreated.context->RemoveOverride(kCodeAddress);
    Require(!loaded, loaded.detail);
    ExecutionResult restored = recreated.context->Execute(kCodeAddress);
    Require(static_cast<bool>(restored), restored.failure.detail);
    Require(restored.value == 42, "removing the native override did not restore guest execution");
    Require(recreated.context->Statistics().translation_invalidations == 2,
            "removing the native override did not invalidate the guest entry");

    DeviceObservations device_observations;
    loaded = recreated.context->RegisterDeviceMemoryRange(
        kDeviceAddress, 0xFFFFF000, 0x1000, nullptr, DeviceWrite, &device_observations);
    Require(loaded.error == RuntimeError::DeviceRangeInvalid,
            "a null device read callback was accepted");
    loaded = recreated.context->RegisterDeviceMemoryRange(
        kDeviceAddress, 0xFFFFF000, 0x1000, DeviceRead, DeviceWrite, &device_observations);
    Require(!loaded, loaded.detail);
    ExecutionResult device_read = recreated.context->Execute(kDeviceReadAddress);
    Require(static_cast<bool>(device_read), device_read.failure.detail);
    Require(device_read.value == 0x12345678U, "the device read returned the wrong value");
    ExecutionResult device_write = recreated.context->Execute(kDeviceWriteAddress);
    Require(static_cast<bool>(device_write), device_write.failure.detail);
    Require(device_write.value == 7, "the device store leaf returned the wrong value");
    Require(device_observations.reads == 1 && device_observations.writes == 1,
            "device callbacks did not run exactly once");
    Require(device_observations.last_write_address == kDeviceAddress + 4 &&
                device_observations.last_write_value == 99,
            "the device write callback observed the wrong address or value");
    Require(recreated.context->Statistics().device_read_calls == 1 &&
                recreated.context->Statistics().device_write_calls == 1,
            "device callback telemetry did not count both accesses");

    const std::uint64_t translations_before_invalidation =
        recreated.context->Statistics().translated_functions;
    loaded = recreated.context->NotifyExecutableWrite(kCodeAddress + 155, 2);
    Require(loaded.error == RuntimeError::ExecutableRangeInvalid,
            "an executable write outside the authenticated code range was accepted");
    loaded = recreated.context->NotifyExecutableWrite(kDeviceReadAddress, 4);
    Require(!loaded, loaded.detail);
    ExecutionResult device_read_after_write = recreated.context->Execute(kDeviceReadAddress);
    Require(static_cast<bool>(device_read_after_write), device_read_after_write.failure.detail);
    Require(device_read_after_write.value == 0x12345678U,
            "the invalidated device read returned the wrong value");
    Require(recreated.context->Statistics().translated_functions ==
                translations_before_invalidation + 1,
            "an executable write did not invalidate the affected translated function");
    ExecutionResult device_write_after_read_invalidation =
        recreated.context->Execute(kDeviceWriteAddress);
    Require(static_cast<bool>(device_write_after_read_invalidation),
            device_write_after_read_invalidation.failure.detail);
    Require(recreated.context->Statistics().translated_functions ==
                translations_before_invalidation + 1,
            "an executable write invalidated an unrelated translated function");

    recreated.context.reset();
    RuntimeCreateResult imported = RuntimeContext::Create();
    Require(static_cast<bool>(imported), imported.failure.detail);
    ImportObservations observations;
    {
        ImportedTestModule imported_module;
        std::array<ImportBinding, 2> bindings{
            ImportBinding{.library = "synthetic.xex",
                          .ordinal = 1U,
                          .kind = ImportKind::Function,
                          .function_handler = FunctionImport,
                          .function_context = &observations},
            ImportBinding{.library = "synthetic.xex",
                          .ordinal = 2U,
                          .kind = ImportKind::Variable,
                          .variable_resolver = RefuseVariable,
                          .variable_resolution_context = &observations},
        };
        loaded = imported.context->LoadModule(imported_module, bindings);
        Require(loaded.error == RuntimeError::VariableResolutionFailed,
                "a null variable resolution did not fail with its typed reason");

        bindings[1].variable_resolver = ResolveVariable;
        loaded = imported.context->LoadModule(imported_module, bindings);
        Require(!loaded, loaded.detail);
    }
    Require(observations.variable_resolutions == 2,
            "variable resolvers did not run exactly once per load attempt");

    ExecutionResult imported_call = imported.context->Execute(kImportedCallAddress);
    Require(static_cast<bool>(imported_call), imported_call.failure.detail);
    Require(imported_call.value == 8, "guest execution did not return across the host import");
    Require(observations.function_calls == 1,
            "guest PPC did not call the bound host function import with its context");

    ExecutionResult variable_call = imported.context->Execute(kVariableCallAddress);
    Require(static_cast<bool>(variable_call), variable_call.failure.detail);
    Require(variable_call.value == 0x12345678U,
            "guest PPC did not load through the bound variable import record");

    std::cout << "runtime contract: Xenia translated and executed one PPC leaf; "
                 "typed function and variable imports crossed Xenia's export machinery\n";
    return 0;
}
