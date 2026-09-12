#include "x360port/runtime.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace
{

using namespace x360port;

constexpr GuestAddress kImportedCallAddress = 0x83000000;
constexpr GuestAddress kFunctionImportAddress = kImportedCallAddress + 0x20;
constexpr GuestAddress kVariableCallAddress = kImportedCallAddress + 0x30;
constexpr GuestAddress kVariableRecordAddress = kImportedCallAddress + 0x48;
constexpr GuestAddress kFunctionRecordAddress = kImportedCallAddress + 0x4C;
constexpr GuestAddress kVariableValueAddress = kImportedCallAddress + 0x58;

struct ImportObservations
{
    std::uint32_t function_calls = 0;
    std::uint64_t last_function_argument = 0;
    GuestAddress scratch_address = 0;
    bool memory_round_trip = false;
    bool invalid_memory_refused = false;
    std::optional<ImportRefusalReason> refusal_reason;
    std::uint32_t variable_resolutions = 0;
};

void FunctionImport(GuestImportContext& call, void* context) noexcept
{
    auto& observations = *static_cast<ImportObservations*>(context);
    ++observations.function_calls;
    observations.last_function_argument = call.argument(0);
    if (observations.refusal_reason)
    {
        call.refuse(*observations.refusal_reason);
        return;
    }
    const std::array<std::byte, 4> expected{std::byte{0x12}, std::byte{0x34}, std::byte{0x56},
                                            std::byte{0x78}};
    observations.memory_round_trip = call.write_memory(observations.scratch_address, expected);
    std::array<std::byte, 4> actual{};
    observations.memory_round_trip = observations.memory_round_trip &&
                                     call.read_memory(observations.scratch_address, actual) &&
                                     actual == expected;
    std::array<std::byte, 1> invalid_read{};
    observations.invalid_memory_refused = !call.read_memory(UINT32_MAX, invalid_read);
    call.set_return_value(call.argument(0));
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
    std::cerr << "import runtime test failed: " << message << '\n';
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
    RuntimeCreateResult imported = RuntimeContext::Create();
    Require(static_cast<bool>(imported), imported.failure.detail);
    ImportObservations observations;
    GuestMemoryAllocationResult import_scratch = imported.context->AllocateGuestMemory(4);
    Require(static_cast<bool>(import_scratch), import_scratch.failure.detail);
    observations.scratch_address = import_scratch.allocation.address;
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
        RuntimeFailure loaded = imported.context->LoadModule(imported_module, bindings);
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
    Require(observations.last_function_argument == 7,
            "the typed function import context did not expose the guest argument");
    Require(observations.memory_round_trip,
            "the typed function import context did not safely round-trip guest memory");
    Require(observations.invalid_memory_refused,
            "the typed function import context accepted an unmapped guest address");

    observations.refusal_reason = ImportRefusalReason::UnsupportedService;
    const ExecutionResult refused_call = imported.context->Execute(kImportedCallAddress);
    Require(
        refused_call.failure.error == RuntimeError::ImportServiceRefused &&
            refused_call.failure.detail.find("synthetic.xex ordinal 1") != std::string::npos &&
            refused_call.failure.detail.find("unsupported service") != std::string::npos,
        "a nested host-import refusal did not stop translated guest execution with its identity");
    const ExecutionResult refused_thunk = imported.context->Execute(kFunctionImportAddress);
    Require(refused_thunk.failure.error == RuntimeError::ImportServiceRefused,
            "a direct host-import refusal did not stop translated guest execution");
    Require(observations.function_calls == 3 &&
                imported.context->Statistics().import_service_refusals == 2,
            "host-import refusals or their denominators were not counted");
    observations.refusal_reason.reset();
    const ExecutionResult resumed_call = imported.context->Execute(kImportedCallAddress);
    Require(static_cast<bool>(resumed_call) && resumed_call.value == 8 &&
                observations.function_calls == 4,
            "a prior import refusal leaked into the following guest call");

    ExecutionResult variable_call = imported.context->Execute(kVariableCallAddress);
    Require(static_cast<bool>(variable_call), variable_call.failure.detail);
    Require(variable_call.value == 0x12345678U,
            "guest PPC did not load through the bound variable import record");

    std::cout << "import runtime contract: typed function, variable, and refusal paths crossed "
                 "Xenia's export machinery\n";
    return 0;
}
