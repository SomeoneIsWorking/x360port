#include "x360port/runtime.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

namespace
{

using namespace x360port;

constexpr GuestAddress kCodeAddress = 0x82000000;
constexpr std::array<std::uint8_t, 8> kReturnFortyTwo{
    0x38, 0x60, 0x00, 0x2A, // li r3, 42
    0x4E, 0x80, 0x00, 0x20, // blr
};

class TestModule final : public GuestModule
{
  public:
    TestModule()
    {
        for (std::size_t index = 0; index < kReturnFortyTwo.size(); ++index)
        {
            image_[index] = static_cast<std::byte>(kReturnFortyTwo[index]);
        }
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
    std::array<std::byte, kReturnFortyTwo.size()> image_{};
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

    std::cout << "runtime contract: Xenia translated and executed one PPC leaf; "
                 "the cache hit reused it and context teardown released the guest image\n";
    return 0;
}
