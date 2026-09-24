#ifndef X360PORT_TESTS_SYNTHETIC_MODULE_HPP
#define X360PORT_TESTS_SYNTHETIC_MODULE_HPP

#include "x360port/runtime.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>

namespace x360port::tests
{

// One authenticated synthetic PPC image shared by the runtime discriminators.
// Each address below names the behaviour its words encode, so a test states
// which guest shape it exercises rather than an opaque offset.
constexpr GuestAddress kCodeAddress = 0x82000000;
constexpr GuestAddress kSelfModifyingAddress = kCodeAddress + 48;
constexpr GuestAddress kInternalCallerAddress = kCodeAddress + 80;
constexpr GuestAddress kInvalidatedCallerAddress = kCodeAddress + 160;
constexpr GuestAddress kNonReturningAddress = kCodeAddress + 108;
constexpr GuestAddress kNonReturningCallerAddress = kCodeAddress + 112;
constexpr GuestAddress kMidCallInvalidationAddress = kCodeAddress + 128;
constexpr GuestAddress kDeviceReadAddress = kCodeAddress + 8;
constexpr GuestAddress kDeviceWriteAddress = kCodeAddress + 24;
constexpr GuestAddress kInvalidOpcodeAddress = kCodeAddress + 176;
constexpr GuestAddress kUnimplementedOpcodeAddress = kCodeAddress + 184;
constexpr GuestAddress kControlFlowAddress = kCodeAddress + 192;
constexpr GuestAddress kMemoryFallbackAddress = kCodeAddress + 216;
constexpr GuestAddress kFaultingReadAddress = kCodeAddress + 232;
// A device range any supported host can protect on its own: 64 KB, the largest
// host page of an ARM64 host.
constexpr std::uint32_t kDeviceAddress = 0xC0010000;
constexpr std::uint32_t kDeviceBytes = 0x10000;
// lis r3, kDeviceAddress@h; ori r3, r3, kDeviceAddress@l
constexpr std::uint32_t kLoadDeviceHigh = 0x3C600000U | (kDeviceAddress >> 16U);
constexpr std::uint32_t kLoadDeviceLow = 0x60630000U | (kDeviceAddress & 0xFFFFU);
class TestModule final : public GuestModule
{
  public:
    TestModule()
    {
        CopyWords(0, {0x3860002A, 0x4E800020}); // li r3, 42; blr
        CopyWords(8, {kLoadDeviceHigh, kLoadDeviceLow, 0x80630000, 0x4E800020});
        CopyWords(
            24, {kLoadDeviceHigh, kLoadDeviceLow, 0x38800063, 0x90830004, 0x38600007, 0x4E800020});
        CopyWords(48, {0x3C608200, 0x60630000, 0x3C803860, 0x6084002B, 0x90830000, 0x38600009,
                       0x4E800020});
        CopyWords(80, {0x7C0802A6, 0x4800000D, 0x7C0803A6, 0x4E800020, 0x38600011, 0x4E800020});
        CopyWords(108, {0x48000000}); // b .
        CopyWords(112, {0x48000009}); // bl +8, to the nested leaf
        CopyWords(120, {0x48000000}); // b .
        CopyWords(128, {0x3C608200, 0x60630098, 0x3C804E80, 0x60840020, 0x90830000, 0x7C6903A6,
                        0x4E800420});
        CopyWords(160, {0x7C0802A6, 0x4BFFFF5D, 0x7C0803A6, 0x4E800020});
        CopyWords(176, {0x00000001, 0x4E800020}); // primary opcode zero is invalid
        CopyWords(184, {0x7C6324AA, 0x4E800020}); // lswi r3,r3,4 is decoded but unsupported by JIT
        CopyWords(192, {0x7C6324AA, 0x2C030000, 0x41820008, 0x38600063, 0x4E800020});
        CopyWords(216, {0x7C8324AA, 0x90830004, 0x80630004, 0x4E800020});
        // li r3,0; lwz r3,0(r3); blr - reads unmapped guest address 0, which
        // is neither MMIO nor a watched page, so Xenia's own handlers decline
        // it and the instruction would otherwise fault forever.
        CopyWords(232, {0x38600000, 0x80630000, 0x4E800020});
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

    std::array<std::byte, 256> image_{};
    ModuleDescriptor descriptor_{};
};

} // namespace x360port::tests

#endif
