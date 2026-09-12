#include "x360port/runtime.hpp"
#include "x360port/xam_input.hpp"

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
constexpr GuestAddress kLoadWordAddress = kImportedCallAddress + 0x60;
constexpr GuestAddress kXamInputImportAddress = kImportedCallAddress + 0x70;

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

struct PadObservations
{
    XamPadSnapshot snapshot;
    std::uint32_t calls = 0;
    std::uint32_t last_user = 0;
    std::uint32_t last_flags = 0;
};

XamPadSnapshot ReadPad(XamInputRequest request, void* context) noexcept
{
    auto& observations = *static_cast<PadObservations*>(context);
    ++observations.calls;
    observations.last_user = request.user_index;
    observations.last_flags = request.flags;
    return observations.snapshot;
}

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
        imports_[2] = {ImportKind::Function,     "xam.xex",
                       kXamInputGetStateOrdinal, "XamInputGetState",
                       kXamInputImportAddress,   kXamInputImportAddress};
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
        constexpr std::array<std::uint8_t, 8> LoadWord{
            0x80, 0x63, 0x00, 0x00, // lwz r3, 0(r3)
            0x4E, 0x80, 0x00, 0x20, // blr
        };
        CopyBytes(kLoadWordAddress - kImportedCallAddress, LoadWord);

        descriptor_.image.sha256 = HashBytes(image_);
        descriptor_.image.base = kImportedCallAddress;
        descriptor_.image.size = static_cast<std::uint32_t>(image_.size());
        descriptor_.image.entry_point = kImportedCallAddress;
        descriptor_.code = {kImportedCallAddress, 0x80U};
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

    std::array<std::byte, 0x90> image_{};
    std::array<ImportRequirement, 3> imports_;
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
    ImportObservations observations;
    PadObservations pad_observations;
    XamInputService xam_input(ReadPad, &pad_observations);
    RuntimeCreateResult imported = RuntimeContext::Create();
    Require(static_cast<bool>(imported), imported.failure.detail);
    GuestMemoryAllocationResult import_scratch = imported.context->AllocateGuestMemory(4);
    Require(static_cast<bool>(import_scratch), import_scratch.failure.detail);
    observations.scratch_address = import_scratch.allocation.address;
    {
        ImportedTestModule imported_module;
        std::array<ImportBinding, 3> bindings{
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
            ImportBinding{.library = "xam.xex",
                          .ordinal = kXamInputGetStateOrdinal,
                          .kind = ImportKind::Function},
        };
        xam_input.Bind(imported_module.ImportManifest()[2], bindings[2]);
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

    GuestMemoryAllocationResult state_memory = imported.context->AllocateGuestMemory(16U);
    Require(static_cast<bool>(state_memory), state_memory.failure.detail);
    const GuestAddress state_address = state_memory.allocation.address;
    pad_observations.snapshot = {.connected = true,
                                 .packet_number = 0x12345678U,
                                 .buttons = 0xABCDU,
                                 .left_trigger = 0x12U,
                                 .right_trigger = 0x34U,
                                 .thumb_lx = -30875,
                                 .thumb_ly = 0x1234,
                                 .thumb_rx = -292,
                                 .thumb_ry = 0x5678};
    const std::array<std::uint64_t, 3> state_arguments{0U, 1U, state_address};
    const ExecutionResult connected =
        imported.context->Execute(kXamInputImportAddress, state_arguments);
    Require(static_cast<bool>(connected) && connected.value == 0U && pad_observations.calls == 1U &&
                pad_observations.last_user == 0U && pad_observations.last_flags == 1U,
            "XamInputGetState did not poll the bound source with the guest arguments");
    constexpr std::array<std::uint32_t, 4> expected_state{0x12345678U, 0xABCD1234U, 0x87651234U,
                                                          0xFEDC5678U};
    for (std::size_t word = 0; word < expected_state.size(); ++word)
    {
        const std::array<std::uint64_t, 1> read_address{state_address +
                                                        static_cast<GuestAddress>(word * 4U)};
        const ExecutionResult loaded_word =
            imported.context->Execute(kLoadWordAddress, read_address);
        Require(static_cast<bool>(loaded_word) && loaded_word.value == expected_state[word],
                "XamInputGetState did not write the big-endian guest state");
    }

    pad_observations.snapshot = {};
    const ExecutionResult disconnected =
        imported.context->Execute(kXamInputImportAddress, state_arguments);
    Require(static_cast<bool>(disconnected) && disconnected.value == kXamInputDeviceNotConnected,
            "a disconnected XAM controller did not return its device status");
    for (std::size_t word = 0; word < expected_state.size(); ++word)
    {
        const std::array<std::uint64_t, 1> read_address{state_address +
                                                        static_cast<GuestAddress>(word * 4U)};
        const ExecutionResult cleared = imported.context->Execute(kLoadWordAddress, read_address);
        Require(static_cast<bool>(cleared) && cleared.value == 0U,
                "a disconnected XAM controller did not clear the prior guest state");
    }

    pad_observations.snapshot.connected = true;
    const std::array<std::uint64_t, 3> query_arguments{0U, 0U, 0U};
    const ExecutionResult query =
        imported.context->Execute(kXamInputImportAddress, query_arguments);
    Require(static_cast<bool>(query) && query.value == 0U,
            "a null XAM state pointer did not query controller connection");
    const std::array<std::uint64_t, 3> invalid_arguments{0U, 0U, UINT32_MAX};
    const ExecutionResult invalid_state =
        imported.context->Execute(kXamInputImportAddress, invalid_arguments);
    Require(invalid_state.failure.error == RuntimeError::ImportServiceRefused &&
                invalid_state.failure.detail.find("xam.xex ordinal 401") != std::string::npos &&
                invalid_state.failure.detail.find("invalid guest memory") != std::string::npos &&
                imported.context->Statistics().import_service_refusals == 3U,
            "an invalid XAM state pointer did not stop translated guest execution");

    std::cout << "import runtime contract: typed function, variable, XAM input, and refusal paths "
                 "crossed "
                 "Xenia's export machinery\n";
    return 0;
}
