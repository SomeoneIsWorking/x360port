#include "xenon_host/host.hpp"

#include "host_run.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

using namespace xenon_host;

void EntryThunk(void*) noexcept {}
void OtherThunk(void*) noexcept {}
void ImportThunk(void*) noexcept {}

class SyntheticModule final : public GuestModule
{
  public:
    SyntheticModule()
        : image(32U), functions{{0x82000004U, EntryThunk}, {0x82000008U, OtherThunk}},
          imports{{"xam", 1U, "XamSynthetic"}, {"xboxkrnl", 2U, "KeSynthetic"}}
    {
        for (std::size_t index = 0; index < image.size(); ++index)
        {
            image[index] = static_cast<std::byte>(index);
        }
        descriptor.image = {
            .sha256 = HashBytes(image),
            .base = 0x82000000U,
            .size = static_cast<std::uint32_t>(image.size()),
            .entry_point = 0x82000004U,
        };
        descriptor.code = {.base = 0x82000004U, .size = 16U};
        ResealFunctionMap();
        ResealImports();
    }

    [[nodiscard]] const ModuleDescriptor& Descriptor() const noexcept override
    {
        return descriptor;
    }

    [[nodiscard]] std::span<const std::byte> ImageBytes() const noexcept override { return image; }

    [[nodiscard]] std::span<const FunctionMapping> FunctionMap() const noexcept override
    {
        return functions;
    }

    [[nodiscard]] std::span<const ImportRequirement> ImportManifest() const noexcept override
    {
        return imports;
    }

    void ResealFunctionMap()
    {
        descriptor.function_count = functions.size();
        descriptor.function_map_sha256 = HashFunctionMap(functions);
    }

    void ResealImports()
    {
        descriptor.import_count = imports.size();
        descriptor.import_manifest_sha256 = HashImportManifest(imports);
    }

    ModuleDescriptor descriptor;
    std::vector<std::byte> image;
    std::vector<FunctionMapping> functions;
    std::vector<ImportRequirement> imports;
};

class SyntheticAdapter final : public TitleAdapter
{
  public:
    explicit SyntheticAdapter(SyntheticModule& guest_module)
        : module(guest_module), bindings{{"xam", 1U, ImportThunk}, {"xboxkrnl", 2U, ImportThunk}}
    {
    }

    [[nodiscard]] std::string_view TitleKey() const noexcept override { return title_key; }

    [[nodiscard]] std::string_view RevisionKey() const noexcept override { return revision_key; }

    [[nodiscard]] const GuestModule& Module() const noexcept override { return module; }

    [[nodiscard]] CapabilitySet Capabilities() const noexcept override { return capabilities; }

    [[nodiscard]] std::span<const ImportBinding> ImportBindings() const noexcept override
    {
        return bindings;
    }

    [[nodiscard]] AdapterRunResult Enter(ValidatedGuestModule validated) noexcept override
    {
        ++entry_calls;
        if (&validated.Module() != &module)
        {
            return {.refusal = "validated token named the wrong module"};
        }
        const GuestMemory& memory = validated.Memory();
        if (memory.Identity().base != module.descriptor.image.base ||
            memory.ImageBytes().size() != module.image.size() ||
            !std::ranges::equal(memory.ImageBytes(), module.image))
        {
            return {.refusal = "validated guest memory did not contain the exact module image"};
        }
        if (refuse_entry)
        {
            return {.refusal = "synthetic adapter refusal"};
        }
        return {.entered_guest = true, .exit_code = 17, .refusal = {}};
    }

    SyntheticModule& module;
    std::string_view title_key = "synthetic-title";
    std::string_view revision_key = "rev-a";
    CapabilitySet capabilities = Capability::GuestMemory | Capability::KernelServices;
    std::vector<ImportBinding> bindings;
    bool refuse_entry = false;
    int entry_calls = 0;
};

struct Fixture
{
    SyntheticModule module;
    SyntheticAdapter adapter{module};
    RunRequest request{.required_capabilities = Capability::GuestMemory};
};

int failures = 0;
int checks = 0;
std::array<bool, static_cast<std::size_t>(RunError::Count)> covered_errors{};

void Expect(Fixture& fixture, RunError expected, std::string_view label)
{
    ++checks;
    covered_errors[static_cast<std::size_t>(expected)] = true;
    const RunResult result = Host::Run(fixture.adapter, fixture.request);
    if (result.error != expected)
    {
        std::fprintf(stderr, "FAIL %-34.*s expected %.*s, got %.*s (%s)\n",
                     static_cast<int>(label.size()), label.data(),
                     static_cast<int>(ToString(expected).size()), ToString(expected).data(),
                     static_cast<int>(ToString(result.error).size()), ToString(result.error).data(),
                     result.detail.c_str());
        ++failures;
    }
    if (expected != RunError::None && result.detail.empty())
    {
        std::fprintf(stderr, "FAIL %-34.*s refusal carried no detail\n",
                     static_cast<int>(label.size()), label.data());
        ++failures;
    }
    const int expected_calls = expected == RunError::None || expected == RunError::AdapterRefused;
    if (fixture.adapter.entry_calls != expected_calls)
    {
        std::fprintf(stderr, "FAIL %-34.*s adapter entry call count %d, expected %d\n",
                     static_cast<int>(label.size()), label.data(), fixture.adapter.entry_calls,
                     expected_calls);
        ++failures;
    }
}

template <typename Mutation>
void Refusal(std::string_view label, RunError expected, Mutation mutation)
{
    Fixture fixture;
    mutation(fixture);
    Expect(fixture, expected, label);
}

void TestAcceptance()
{
    Fixture fixture;
    const RunResult result = Host::Run(fixture.adapter, fixture.request);
    ++checks;
    covered_errors[static_cast<std::size_t>(RunError::None)] = true;
    if (!result || result.guest_exit_code != 17 || fixture.adapter.entry_calls != 1)
    {
        std::fprintf(stderr, "FAIL acceptance: error=%.*s exit=%d calls=%d detail=%s\n",
                     static_cast<int>(ToString(result.error).size()), ToString(result.error).data(),
                     result.guest_exit_code, fixture.adapter.entry_calls, result.detail.c_str());
        ++failures;
    }
}

[[nodiscard]] std::byte* RefuseReservation(std::uint64_t) noexcept { return nullptr; }

[[nodiscard]] std::byte* MisalignedReservation(std::uint64_t) noexcept
{
    return reinterpret_cast<std::byte*>(0x21U);
}

alignas(GuestMemory::RequiredAlignment) std::array<std::byte, 32> fake_window{};

[[nodiscard]] std::byte* FakeReservation(std::uint64_t) noexcept { return fake_window.data(); }

[[nodiscard]] bool RefuseCommit(std::byte*, GuestMemoryRange) noexcept { return false; }

void IgnoreRelease(std::byte*, std::uint64_t) noexcept {}

void TestGuestMemoryHostRefusals()
{
    const auto expect_refusal = [](RunError expected, const GuestVirtualMemoryOps& operations)
    {
        Fixture fixture;
        ++checks;
        covered_errors[static_cast<std::size_t>(expected)] = true;
        const RunResult result = HostRunner::Run(fixture.adapter, fixture.request, operations);
        if (result.error != expected || result.detail.empty() || fixture.adapter.entry_calls != 0)
        {
            std::fprintf(stderr,
                         "FAIL guest memory host refusal: expected=%.*s error=%.*s calls=%d "
                         "detail=%s\n",
                         static_cast<int>(ToString(expected).size()), ToString(expected).data(),
                         static_cast<int>(ToString(result.error).size()),
                         ToString(result.error).data(), fixture.adapter.entry_calls,
                         result.detail.c_str());
            ++failures;
        }
    };

    expect_refusal(
        RunError::GuestMemoryReservationFailed,
        {.reserve = RefuseReservation, .commit = RefuseCommit, .release = IgnoreRelease});
    expect_refusal(
        RunError::InvalidGuestMemoryAlignment,
        {.reserve = MisalignedReservation, .commit = RefuseCommit, .release = IgnoreRelease});
    expect_refusal(RunError::GuestMemoryCommitFailed,
                   {.reserve = FakeReservation, .commit = RefuseCommit, .release = IgnoreRelease});
}

void TestRunErrorCoverage()
{
    for (std::size_t index = 0; index < covered_errors.size(); ++index)
    {
        if (!covered_errors[index])
        {
            std::fprintf(stderr, "FAIL RunError value %zu has no acceptance/refusal test\n", index);
            ++failures;
        }
    }
}

void TestSha256KnownAnswer()
{
    constexpr std::array Input = {std::byte{0x61}, std::byte{0x62}, std::byte{0x63}};
    constexpr Sha256Digest Expected = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40,
        0xde, 0x5d, 0xae, 0x22, 0x23, 0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17,
        0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad,
    };
    ++checks;
    if (HashBytes(Input) != Expected)
    {
        std::fprintf(stderr, "FAIL SHA-256 known-answer test for abc\n");
        ++failures;
    }
}

void TestRefusals()
{
    Refusal("invalid title key", RunError::InvalidTitleKey,
            [](Fixture& value) { value.adapter.title_key = "../title"; });
    Refusal("invalid revision key", RunError::InvalidRevisionKey,
            [](Fixture& value) { value.adapter.revision_key = ""; });
    Refusal("invalid capability bits", RunError::InvalidCapabilitySet, [](Fixture& value)
            { value.adapter.capabilities = CapabilitySet::FromBits(0x80000000U); });
    Refusal("missing capability", RunError::MissingCapability,
            [](Fixture& value) { value.request.required_capabilities = Capability::Graphics; });
    Refusal("zero image digest", RunError::InvalidImageDigest,
            [](Fixture& value) { value.module.descriptor.image.sha256 = {}; });
    Refusal("image size mismatch", RunError::ImageSizeMismatch,
            [](Fixture& value) { --value.module.descriptor.image.size; });
    Refusal("image address overflow", RunError::ImageAddressOverflow,
            [](Fixture& value) { value.module.descriptor.image.base = 0xfffffff0U; });
    Refusal("image digest mismatch", RunError::ImageDigestMismatch,
            [](Fixture& value) { value.module.descriptor.image.sha256[0] ^= 0xffU; });
    Refusal("invalid code range", RunError::InvalidCodeRange,
            [](Fixture& value) { value.module.descriptor.code.size = 0; });
    Refusal("entry outside code", RunError::EntryPointOutsideCode, [](Fixture& value)
            { value.module.descriptor.image.entry_point = value.module.descriptor.image.base; });
    Refusal("function count mismatch", RunError::FunctionCountMismatch,
            [](Fixture& value) { ++value.module.descriptor.function_count; });
    Refusal("empty function map", RunError::EmptyFunctionMap,
            [](Fixture& value)
            {
                value.module.functions.clear();
                value.module.ResealFunctionMap();
            });
    Refusal("invalid function address", RunError::InvalidFunctionAddress,
            [](Fixture& value)
            {
                value.module.functions[0].address += 1U;
                value.module.ResealFunctionMap();
            });
    Refusal("null function thunk", RunError::NullFunctionThunk,
            [](Fixture& value) { value.module.functions[0].thunk = nullptr; });
    Refusal("unsorted function map", RunError::UnsortedFunctionMap,
            [](Fixture& value)
            {
                std::swap(value.module.functions[0], value.module.functions[1]);
                value.module.ResealFunctionMap();
            });
    Refusal("entry point missing", RunError::EntryPointMissing,
            [](Fixture& value)
            {
                value.module.functions = {
                    {0x82000008U, EntryThunk},
                    {0x8200000cU, OtherThunk},
                };
                value.module.ResealFunctionMap();
            });
    Refusal("function digest mismatch", RunError::FunctionMapDigestMismatch,
            [](Fixture& value) { value.module.descriptor.function_map_sha256[0] ^= 0xffU; });
    Refusal("import count mismatch", RunError::ImportCountMismatch,
            [](Fixture& value) { ++value.module.descriptor.import_count; });
    Refusal("invalid import", RunError::InvalidImport,
            [](Fixture& value)
            {
                value.module.imports[0].library = "";
                value.module.ResealImports();
            });
    Refusal("unsorted imports", RunError::UnsortedImportManifest,
            [](Fixture& value)
            {
                std::swap(value.module.imports[0], value.module.imports[1]);
                value.module.ResealImports();
            });
    Refusal("import digest mismatch", RunError::ImportManifestDigestMismatch,
            [](Fixture& value) { value.module.descriptor.import_manifest_sha256[0] ^= 0xffU; });
    Refusal("binding count mismatch", RunError::ImportBindingCountMismatch,
            [](Fixture& value) { value.adapter.bindings.pop_back(); });
    Refusal("binding mismatch", RunError::ImportBindingMismatch,
            [](Fixture& value) { ++value.adapter.bindings[0].ordinal; });
    Refusal("null import handler", RunError::NullImportHandler,
            [](Fixture& value) { value.adapter.bindings[0].handler = nullptr; });
    Refusal("adapter refusal", RunError::AdapterRefused,
            [](Fixture& value) { value.adapter.refuse_entry = true; });
}

} // namespace

int main()
{
    TestSha256KnownAnswer();
    TestAcceptance();
    TestRefusals();
    TestGuestMemoryHostRefusals();
    TestRunErrorCoverage();
    if (failures != 0)
    {
        std::fprintf(stderr, "%d failure(s) across %d contract checks\n", failures, checks);
        return 1;
    }
    std::printf("%d checks passed (SHA-256 KAT, 1 acceptance, 28 refusal paths)\n", checks);
    return 0;
}
