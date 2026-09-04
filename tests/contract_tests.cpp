#include "x360port/validation.hpp"

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

using namespace x360port;

void ImportThunk(void*, void*, void*) noexcept {}
[[nodiscard]] GuestAddress ResolveVariable(void*) noexcept { return 0x8200001cU; }

class SyntheticModule final : public GuestModule
{
  public:
    SyntheticModule()
        : image(32U),
          imports{{ImportKind::Function, "xam", 1U, "XamSynthetic", 0x82000008U, 0x82000014U},
                  {ImportKind::Variable, "xboxkrnl", 2U, "KeSynthetic", 0x82000018U, 0x82000018U}}
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
        descriptor.code = {.base = 0x82000004U, .size = 20U};
        ResealImports();
    }

    [[nodiscard]] const ModuleDescriptor& Descriptor() const noexcept override
    {
        return descriptor;
    }
    [[nodiscard]] std::span<const std::byte> ImageBytes() const noexcept override { return image; }
    [[nodiscard]] std::span<const ImportRequirement> ImportManifest() const noexcept override
    {
        return imports;
    }
    void ResealImports()
    {
        descriptor.import_count = imports.size();
        descriptor.import_manifest_sha256 = HashImportManifest(imports);
    }

    ModuleDescriptor descriptor;
    std::vector<std::byte> image;
    std::vector<ImportRequirement> imports;
};

struct Fixture
{
    SyntheticModule module;
    std::vector<ImportBinding> bindings{
        {.library = "xam",
         .ordinal = 1U,
         .kind = ImportKind::Function,
         .function_handler = ImportThunk},
        {.library = "xboxkrnl",
         .ordinal = 2U,
         .kind = ImportKind::Variable,
         .variable_resolver = ResolveVariable},
    };
};

int failures = 0;
int checks = 0;
std::array<bool, static_cast<std::size_t>(ValidationError::Count)> covered_errors{};

void CheckResult(const ValidationResult& result, ValidationError expected, std::string_view label)
{
    ++checks;
    covered_errors[static_cast<std::size_t>(expected)] = true;
    if (result.error != expected)
    {
        std::fprintf(stderr, "FAIL %-34.*s expected %.*s, got %.*s (%s)\n",
                     static_cast<int>(label.size()), label.data(),
                     static_cast<int>(ToString(expected).size()), ToString(expected).data(),
                     static_cast<int>(ToString(result.error).size()), ToString(result.error).data(),
                     result.detail.c_str());
        ++failures;
    }
    if (expected != ValidationError::None && result.detail.empty())
    {
        std::fprintf(stderr, "FAIL %-34.*s refusal carried no detail\n",
                     static_cast<int>(label.size()), label.data());
        ++failures;
    }
}

template <typename Mutation>
void ModuleRefusal(std::string_view label, ValidationError expected, Mutation mutation)
{
    Fixture fixture;
    mutation(fixture);
    CheckResult(ValidateModule(fixture.module), expected, label);
}

template <typename Mutation>
void BindingRefusal(std::string_view label, ValidationError expected, Mutation mutation)
{
    Fixture fixture;
    mutation(fixture);
    CheckResult(ValidateImports(fixture.module, fixture.bindings), expected, label);
}

void TestAcceptance()
{
    Fixture fixture;
    CheckResult(ValidateModule(fixture.module), ValidationError::None, "module acceptance");
    CheckResult(ValidateImports(fixture.module, fixture.bindings), ValidationError::None,
                "import acceptance");
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

void TestImportDigestCoverage()
{
    Fixture fixture;
    const Sha256Digest baseline = HashImportManifest(fixture.module.imports);
    constexpr Sha256Digest Expected = {
        0x24, 0xb2, 0xb7, 0x79, 0xe0, 0x32, 0x1d, 0x1b, 0x10, 0x69, 0x5c,
        0x83, 0x23, 0x91, 0x69, 0xe3, 0xce, 0xc4, 0xb8, 0xf2, 0x83, 0x4b,
        0x45, 0xc3, 0x81, 0x94, 0xd5, 0x09, 0xb7, 0xf2, 0x88, 0x16,
    };
    ++checks;
    if (baseline != Expected)
    {
        std::fprintf(stderr, "FAIL canonical import-manifest known-answer test\n");
        ++failures;
    }
    const auto expect_changed = [&baseline](Fixture& candidate, std::string_view label)
    {
        ++checks;
        if (HashImportManifest(candidate.module.imports) == baseline)
        {
            std::fprintf(stderr, "FAIL import digest did not seal %.*s\n",
                         static_cast<int>(label.size()), label.data());
            ++failures;
        }
    };

    Fixture kind;
    kind.module.imports[0].kind = ImportKind::Variable;
    expect_changed(kind, "kind");
    Fixture library;
    library.module.imports[0].library = "xam-mutated";
    expect_changed(library, "library");
    Fixture ordinal;
    ++ordinal.module.imports[0].ordinal;
    expect_changed(ordinal, "ordinal");
    Fixture name;
    name.module.imports[0].name = "XamMutated";
    expect_changed(name, "name");
    Fixture address;
    address.module.imports[0].address += 4U;
    expect_changed(address, "guest address");
    Fixture record_address;
    record_address.module.imports[0].record_address += 4U;
    expect_changed(record_address, "record address");
}

void TestModuleRefusals()
{
    ModuleRefusal("zero image digest", ValidationError::InvalidImageDigest,
                  [](Fixture& value) { value.module.descriptor.image.sha256 = {}; });
    ModuleRefusal("image size mismatch", ValidationError::ImageSizeMismatch,
                  [](Fixture& value) { --value.module.descriptor.image.size; });
    ModuleRefusal("image address overflow", ValidationError::ImageAddressOverflow,
                  [](Fixture& value) { value.module.descriptor.image.base = 0xfffffff0U; });
    ModuleRefusal("image digest mismatch", ValidationError::ImageDigestMismatch,
                  [](Fixture& value) { value.module.image[0] ^= std::byte{0xff}; });
    ModuleRefusal("invalid code range", ValidationError::InvalidCodeRange,
                  [](Fixture& value) { value.module.descriptor.code.size = 0; });
    ModuleRefusal(
        "entry outside code", ValidationError::EntryPointOutsideCode, [](Fixture& value)
        { value.module.descriptor.image.entry_point = value.module.descriptor.image.base; });
    ModuleRefusal("import count mismatch", ValidationError::ImportCountMismatch,
                  [](Fixture& value) { ++value.module.descriptor.import_count; });
    ModuleRefusal("invalid import", ValidationError::InvalidImport,
                  [](Fixture& value)
                  {
                      value.module.imports[0].library = "";
                      value.module.ResealImports();
                  });
    ModuleRefusal("import ordinal out of range", ValidationError::ImportOrdinalOutOfRange,
                  [](Fixture& value)
                  {
                      value.module.imports[0].ordinal = 0x10000U;
                      value.module.ResealImports();
                  });
    ModuleRefusal("conflicting import address", ValidationError::ImportAddressConflict,
                  [](Fixture& value)
                  {
                      value.module.imports[1].address = value.module.imports[0].address;
                      value.module.ResealImports();
                  });
    ModuleRefusal("conflicting import record", ValidationError::ImportAddressConflict,
                  [](Fixture& value)
                  {
                      value.module.imports[1].record_address =
                          value.module.imports[0].record_address;
                      value.module.ResealImports();
                  });
    ModuleRefusal("invalid import kind", ValidationError::InvalidImport,
                  [](Fixture& value)
                  {
                      value.module.imports[0].kind = static_cast<ImportKind>(0xffU);
                      value.module.ResealImports();
                  });
    ModuleRefusal("function import outside code", ValidationError::InvalidImport,
                  [](Fixture& value)
                  {
                      value.module.imports[0].address = value.module.descriptor.image.base;
                      value.module.ResealImports();
                  });
    ModuleRefusal("import record outside image", ValidationError::InvalidImport,
                  [](Fixture& value)
                  {
                      value.module.imports[0].record_address =
                          value.module.descriptor.image.base + value.module.descriptor.image.size;
                      value.module.ResealImports();
                  });
    ModuleRefusal("variable address mismatch", ValidationError::InvalidImport,
                  [](Fixture& value)
                  {
                      value.module.imports[1].address += 4U;
                      value.module.ResealImports();
                  });
    ModuleRefusal("unsorted imports", ValidationError::UnsortedImportManifest,
                  [](Fixture& value)
                  {
                      std::swap(value.module.imports[0], value.module.imports[1]);
                      value.module.ResealImports();
                  });
    ModuleRefusal("import digest mismatch", ValidationError::ImportManifestDigestMismatch,
                  [](Fixture& value)
                  { value.module.descriptor.import_manifest_sha256[0] ^= 0xffU; });
}

void TestBindingRefusals()
{
    BindingRefusal("binding count mismatch", ValidationError::ImportBindingCountMismatch,
                   [](Fixture& value) { value.bindings.pop_back(); });
    BindingRefusal("binding mismatch", ValidationError::ImportBindingMismatch,
                   [](Fixture& value) { ++value.bindings[0].ordinal; });
    BindingRefusal("binding kind mismatch", ValidationError::ImportBindingKindMismatch,
                   [](Fixture& value) { value.bindings[0].kind = ImportKind::Variable; });
    BindingRefusal("null function handler", ValidationError::ImportBindingCallbackMismatch,
                   [](Fixture& value) { value.bindings[0].function_handler = nullptr; });
    BindingRefusal("function has resolver", ValidationError::ImportBindingCallbackMismatch,
                   [](Fixture& value) { value.bindings[0].variable_resolver = ResolveVariable; });
    BindingRefusal("function has variable context", ValidationError::ImportBindingCallbackMismatch,
                   [](Fixture& value) { value.bindings[0].variable_resolution_context = &value; });
    BindingRefusal("null variable resolver", ValidationError::ImportBindingCallbackMismatch,
                   [](Fixture& value) { value.bindings[1].variable_resolver = nullptr; });
    BindingRefusal("variable has handler", ValidationError::ImportBindingCallbackMismatch,
                   [](Fixture& value) { value.bindings[1].function_handler = ImportThunk; });
    BindingRefusal("variable has function context", ValidationError::ImportBindingCallbackMismatch,
                   [](Fixture& value) { value.bindings[1].function_context = &value; });
}

void TestErrorCoverage()
{
    for (std::size_t index = 0; index < covered_errors.size(); ++index)
    {
        if (!covered_errors[index])
        {
            std::fprintf(stderr, "FAIL ValidationError value %zu has no test\n", index);
            ++failures;
        }
    }
}

} // namespace

int main()
{
    TestSha256KnownAnswer();
    TestImportDigestCoverage();
    TestAcceptance();
    TestModuleRefusals();
    TestBindingRefusals();
    TestErrorCoverage();
    if (failures != 0)
    {
        std::fprintf(stderr, "%d failure(s) across %d contract checks\n", failures, checks);
        return 1;
    }
    std::printf("%d checks passed (SHA-256, image layout, typed imports, refusals)\n", checks);
    return 0;
}
