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
constexpr GuestAddress kInvalidatedCallerAddress = kCodeAddress + 160;
constexpr GuestAddress kNonReturningAddress = kCodeAddress + 108;
constexpr GuestAddress kNonReturningCallerAddress = kCodeAddress + 112;
constexpr GuestAddress kMidCallInvalidationAddress = kCodeAddress + 128;
constexpr GuestAddress kDeviceReadAddress = kCodeAddress + 8;
constexpr GuestAddress kDeviceWriteAddress = kCodeAddress + 24;
constexpr std::uint32_t kDeviceAddress = 0xC0001000;
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
        CopyWords(160, {0x7C0802A6, 0x4BFFFF5D, 0x7C0803A6, 0x4E800020});
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

    std::array<std::byte, 176> image_{};
    ModuleDescriptor descriptor_{};
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

    ExecutionResult invalidated_caller = created.context->Execute(kInvalidatedCallerAddress);
    Require(static_cast<bool>(invalidated_caller) && invalidated_caller.value == 42,
            "the caller of the original leaf did not return its value");

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
    ExecutionResult caller_after_write = created.context->Execute(kInvalidatedCallerAddress);
    Require(static_cast<bool>(caller_after_write) && caller_after_write.value == 43,
            "a cached guest caller reused the pre-invalidation callee body");
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
    loaded = recreated.context->NotifyExecutableWrite(
        kCodeAddress + module.Descriptor().code.size - 1U, 2);
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

    std::cout << "runtime contract: Xenia translated and executed authenticated PPC with bounded "
                 "calls, overrides, device memory, and executable invalidation\n";
    return 0;
}
