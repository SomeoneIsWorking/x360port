#include "x360port/import_claims.hpp"
#include "x360port/runtime.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <string_view>

namespace
{

using namespace x360port;

// The guest ABI values this test drives and expects. They are restated from
// the console's documented MEM_/PAGE_/STATUS_ constants rather than taken from
// the same header the service uses, so a wrong constant in the service fails
// here instead of agreeing with itself.
constexpr std::uint32_t X_MEM_COMMIT = 0x00001000U;
constexpr std::uint32_t X_MEM_RESERVE = 0x00002000U;
constexpr std::uint32_t X_MEM_DECOMMIT = 0x00004000U;
constexpr std::uint32_t X_MEM_RELEASE = 0x00008000U;
constexpr std::uint32_t X_MEM_FREE = 0x00010000U;
constexpr std::uint32_t X_MEM_RESET = 0x00080000U;
constexpr std::uint32_t X_PAGE_READWRITE = 0x00000004U;
constexpr std::uint64_t X_STATUS_SUCCESS = 0x00000000U;
constexpr std::uint64_t X_STATUS_UNSUCCESSFUL = 0xC0000001U;
constexpr std::uint64_t X_STATUS_INVALID_PARAMETER = 0xC000000DU;
constexpr std::uint64_t X_STATUS_MEMORY_NOT_ALLOCATED = 0xC00000A0U;

constexpr GuestAddress kImageAddress = 0x84000000;
constexpr std::uint32_t kImageSize = 0x40;
constexpr std::uint32_t kThunkStride = 0x10;
constexpr std::size_t kServiceCount = 3;
constexpr std::size_t kRegionInfoBytes = 28;
constexpr std::uint32_t kLargePageSize = 64U * 1024U;
// An address in the physical window, which is not a guest-virtual heap. The
// service must refuse it rather than allocating from the wrong owner.
constexpr std::uint32_t kPhysicalWindowAddress = 0xA0000000;

[[noreturn]] void Fail(std::string_view message)
{
    std::cerr << "kernel memory test failed: " << message << '\n';
    std::exit(1);
}

void Require(bool condition, std::string_view message)
{
    if (!condition)
    {
        Fail(message);
    }
}

// The export table is the single authority for these ordinals; naming them here
// would reintroduce the literals the claim table exists to remove.
[[nodiscard]] std::uint32_t KernelOrdinal(std::string_view name)
{
    const auto found = ExportNames::Find(ExportNames::Library::Kernel, name);
    if (!found.has_value())
    {
        Fail("xboxkrnl.exe does not export a service this test requires");
    }
    return found->ordinal;
}

// The three virtual-memory exports, ordered by ordinal because the module
// contract requires a sorted import manifest.
[[nodiscard]] std::array<std::string_view, kServiceCount> OrderedServiceNames()
{
    std::array<std::string_view, kServiceCount> names{
        "NtAllocateVirtualMemory", "NtFreeVirtualMemory", "NtQueryVirtualMemory"};
    std::ranges::sort(names, {}, [](std::string_view name) { return KernelOrdinal(name); });
    return names;
}

class KernelMemoryModule final : public GuestModule
{
  public:
    KernelMemoryModule()
    {
        const std::array<std::string_view, kServiceCount> names = OrderedServiceNames();
        for (std::size_t index = 0; index < names.size(); ++index)
        {
            const GuestAddress thunk = ThunkAddress(index);
            imports_[index] = {ImportKind::Function, "xboxkrnl.exe", KernelOrdinal(names[index]),
                               names[index],         thunk,          thunk};
        }
        names_ = names;

        // blr, so the entry point is a real instruction inside the code range.
        image_[0] = std::byte{0x4E};
        image_[1] = std::byte{0x80};
        image_[2] = std::byte{0x00};
        image_[3] = std::byte{0x20};

        descriptor_.image.sha256 = HashBytes(image_);
        descriptor_.image.base = kImageAddress;
        descriptor_.image.size = kImageSize;
        descriptor_.image.entry_point = kImageAddress;
        descriptor_.code = {kImageAddress, kImageSize};
        descriptor_.import_count = imports_.size();
        descriptor_.import_manifest_sha256 = HashImportManifest(imports_);
    }

    [[nodiscard]] static GuestAddress ThunkAddress(std::size_t index) noexcept
    {
        return kImageAddress + static_cast<GuestAddress>((index + 1U) * kThunkStride);
    }

    [[nodiscard]] GuestAddress ThunkFor(std::string_view name) const
    {
        for (std::size_t index = 0; index < names_.size(); ++index)
        {
            if (names_[index] == name)
            {
                return ThunkAddress(index);
            }
        }
        Fail("the synthetic kernel module does not import that service");
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
    std::array<std::byte, kImageSize> image_{};
    std::array<ImportRequirement, kServiceCount> imports_{};
    std::array<std::string_view, kServiceCount> names_{};
    ModuleDescriptor descriptor_{};
};

// The in/out base and size words every virtual-memory call reads and writes.
class ArgumentBlock final
{
  public:
    ArgumentBlock(RuntimeContext& runtime, GuestAddress address) noexcept
        : runtime_(&runtime), address_(address)
    {
    }

    [[nodiscard]] GuestAddress base_pointer() const noexcept { return address_; }
    [[nodiscard]] GuestAddress size_pointer() const noexcept { return address_ + 4U; }

    void Set(std::uint32_t base, std::uint32_t size) const
    {
        Store(address_, base);
        Store(address_ + 4U, size);
    }

    [[nodiscard]] std::uint32_t base() const { return Load(address_); }
    [[nodiscard]] std::uint32_t size() const { return Load(address_ + 4U); }

  private:
    void Store(GuestAddress address, std::uint32_t value) const
    {
        const std::array<std::byte, 4> bytes{
            static_cast<std::byte>(value >> 24U), static_cast<std::byte>(value >> 16U),
            static_cast<std::byte>(value >> 8U), static_cast<std::byte>(value)};
        const RuntimeFailure failure = runtime_->WriteGuestMemory(address, bytes);
        Require(!failure, failure.detail);
    }

    [[nodiscard]] std::uint32_t Load(GuestAddress address) const
    {
        std::array<std::byte, 4> bytes{};
        const RuntimeFailure failure = runtime_->ReadGuestMemory(address, bytes);
        Require(!failure, failure.detail);
        return (static_cast<std::uint32_t>(bytes[0]) << 24U) |
               (static_cast<std::uint32_t>(bytes[1]) << 16U) |
               (static_cast<std::uint32_t>(bytes[2]) << 8U) | static_cast<std::uint32_t>(bytes[3]);
    }

    RuntimeContext* runtime_;
    GuestAddress address_;
};

[[nodiscard]] std::array<std::uint32_t, 7> ReadRegionInfo(RuntimeContext& runtime,
                                                          GuestAddress address)
{
    std::array<std::byte, kRegionInfoBytes> bytes{};
    const RuntimeFailure failure = runtime.ReadGuestMemory(address, bytes);
    Require(!failure, failure.detail);
    std::array<std::uint32_t, 7> words{};
    for (std::size_t word = 0; word < words.size(); ++word)
    {
        const std::size_t offset = word * 4U;
        words[word] = (static_cast<std::uint32_t>(bytes[offset]) << 24U) |
                      (static_cast<std::uint32_t>(bytes[offset + 1U]) << 16U) |
                      (static_cast<std::uint32_t>(bytes[offset + 2U]) << 8U) |
                      static_cast<std::uint32_t>(bytes[offset + 3U]);
    }
    return words;
}

} // namespace

int main()
{
    RuntimeCreateResult created = RuntimeContext::Create();
    Require(static_cast<bool>(created), created.failure.detail);
    RuntimeContext& runtime = *created.context;

    const KernelMemoryModule module;
    std::array<ImportBinding, kServiceCount> bindings{};
    ImportClaimTable claims;
    const RuntimeFailure resolved = claims.Resolve(runtime.KernelServiceClaims());
    Require(!resolved, resolved.detail);
    for (std::size_t index = 0; index < bindings.size(); ++index)
    {
        const ImportRequirement& requirement = module.ImportManifest()[index];
        bindings[index] = ImportBinding{.library = requirement.library,
                                        .ordinal = requirement.ordinal,
                                        .kind = requirement.kind};
        claims.Apply(requirement, bindings[index]);
    }
    Require(claims.applied() == kServiceCount,
            "the runtime's kernel claims did not install every virtual-memory handler");
    const RuntimeFailure loaded = runtime.LoadModule(module, bindings);
    Require(!loaded, loaded.detail);

    const GuestAddress allocate = module.ThunkFor("NtAllocateVirtualMemory");
    const GuestAddress free = module.ThunkFor("NtFreeVirtualMemory");
    const GuestAddress query = module.ThunkFor("NtQueryVirtualMemory");

    GuestMemoryAllocationResult scratch = runtime.AllocateGuestMemory(kRegionInfoBytes + 8U);
    Require(static_cast<bool>(scratch), scratch.failure.detail);
    const ArgumentBlock arguments(runtime, scratch.allocation.address);
    const GuestAddress info_address = scratch.allocation.address + 8U;

    // A kernel-chosen commit succeeds and reports the page-rounded size back.
    arguments.Set(0U, 0x1000U);
    const std::array<std::uint64_t, 5> commit{arguments.base_pointer(), arguments.size_pointer(),
                                              X_MEM_COMMIT | X_MEM_RESERVE, X_PAGE_READWRITE, 0U};
    ExecutionResult call = runtime.Execute(allocate, commit);
    Require(static_cast<bool>(call), call.failure.detail);
    Require(call.value == X_STATUS_SUCCESS, "a committed allocation did not report success");
    const std::uint32_t address = arguments.base();
    Require(address != 0U, "a committed allocation did not report its guest address");
    Require(arguments.size() == kLargePageSize,
            "a kernel-chosen allocation did not round its region up to the 64 KiB granularity");

    // The allocation is readable and writable through the same guest mapping
    // the title would use, which is what a commit must guarantee.
    const std::array<std::byte, 4> pattern{std::byte{0xDE}, std::byte{0xAD}, std::byte{0xBE},
                                           std::byte{0xEF}};
    const RuntimeFailure mapped = runtime.WriteMappedGuestMemory(address, pattern);
    Require(!mapped, mapped.detail);

    const std::array<std::uint64_t, 3> query_arguments{address, info_address, 0U};
    call = runtime.Execute(query, query_arguments);
    Require(static_cast<bool>(call) && call.value == X_STATUS_SUCCESS, "a query of a live "
                                                                       "allocation failed");
    std::array<std::uint32_t, 7> info = ReadRegionInfo(runtime, info_address);
    Require(info[0] == address, "the queried region did not report its own base address");
    Require(info[4] == X_MEM_COMMIT, "a committed region was not reported as committed");
    Require(info[5] == X_PAGE_READWRITE,
            "the queried region did not report the protection it was allocated with");

    // Decommit keeps the reservation, so the range stays owned by the guest.
    arguments.Set(address, kLargePageSize);
    const std::array<std::uint64_t, 4> decommit{arguments.base_pointer(), arguments.size_pointer(),
                                                X_MEM_DECOMMIT, 0U};
    call = runtime.Execute(free, decommit);
    Require(static_cast<bool>(call) && call.value == X_STATUS_SUCCESS, "a decommit failed");
    call = runtime.Execute(query, query_arguments);
    Require(static_cast<bool>(call) && call.value == X_STATUS_SUCCESS, "a query after decommit "
                                                                       "failed");
    info = ReadRegionInfo(runtime, info_address);
    Require(info[4] == X_MEM_RESERVE,
            "a decommitted range did not keep the reservation the guest still owns");

    // Release returns the range; the same address then reports as free.
    arguments.Set(address, 0U);
    const std::array<std::uint64_t, 4> release{arguments.base_pointer(), arguments.size_pointer(),
                                               X_MEM_RELEASE, 0U};
    call = runtime.Execute(free, release);
    Require(static_cast<bool>(call) && call.value == X_STATUS_SUCCESS, "a release failed");
    call = runtime.Execute(query, query_arguments);
    Require(static_cast<bool>(call) && call.value == X_STATUS_SUCCESS, "a query after release "
                                                                       "failed");
    info = ReadRegionInfo(runtime, info_address);
    Require(info[4] == X_MEM_FREE, "a released range was not reported as free");

    // Re-committing the released range must hand back zeroed pages. A fresh
    // mapping is zero anyway, so only reuse of a range the guest wrote can tell
    // a service that zeroes from one that does not.
    arguments.Set(address, 0x1000U);
    call = runtime.Execute(allocate, commit);
    Require(static_cast<bool>(call) && call.value == X_STATUS_SUCCESS,
            "a fixed re-commit of the released range failed");
    Require(arguments.base() == address,
            "a fixed re-commit did not return the address it was asked for");
    std::array<std::byte, 4> reused{};
    const RuntimeFailure reread = runtime.ReadMappedGuestMemory(address, reused);
    Require(!reread, reread.detail);
    Require(reused == std::array<std::byte, 4>{},
            "a re-committed range was handed back holding the guest's earlier bytes");

    arguments.Set(address, 0U);
    call = runtime.Execute(free, release);
    Require(static_cast<bool>(call) && call.value == X_STATUS_SUCCESS,
            "releasing the re-committed range failed");

    // Releasing it twice is reported, not swallowed: answering success would
    // tell the title's own wrapper that a free it never performed worked.
    arguments.Set(address, 0U);
    call = runtime.Execute(free, release);
    Require(static_cast<bool>(call) && call.value == X_STATUS_UNSUCCESSFUL,
            "a second release of the same range reported success");

    // A free of address zero is ordinary guest traffic and must answer without
    // consulting a heap.
    arguments.Set(0U, 0U);
    call = runtime.Execute(free, release);
    Require(static_cast<bool>(call) && call.value == X_STATUS_MEMORY_NOT_ALLOCATED,
            "a free of address zero was not answered as an unallocated address");

    // A fixed base outside any guest-virtual heap belongs to another owner.
    arguments.Set(kPhysicalWindowAddress, 0x1000U);
    call = runtime.Execute(allocate, commit);
    Require(static_cast<bool>(call) && call.value == X_STATUS_INVALID_PARAMETER,
            "an allocation at a physical-window address was not refused");

    arguments.Set(0U, 0U);
    call = runtime.Execute(allocate, commit);
    Require(static_cast<bool>(call) && call.value == X_STATUS_INVALID_PARAMETER,
            "a zero-sized allocation was not refused");

    arguments.Set(0U, 0x1000U);
    const std::array<std::uint64_t, 5> no_type{arguments.base_pointer(), arguments.size_pointer(),
                                               0U, X_PAGE_READWRITE, 0U};
    call = runtime.Execute(allocate, no_type);
    Require(static_cast<bool>(call) && call.value == X_STATUS_INVALID_PARAMETER,
            "an allocation with no allocation type was not refused");

    const std::array<std::uint64_t, 5> null_pointers{0U, 0U, X_MEM_COMMIT, X_PAGE_READWRITE, 0U};
    call = runtime.Execute(allocate, null_pointers);
    Require(static_cast<bool>(call) && call.value == X_STATUS_INVALID_PARAMETER,
            "an allocation with null in/out pointers was not refused");

    // MEM_RESET is not implemented, so the call refuses by name rather than
    // reporting a success the guest's pages would not reflect.
    const std::array<std::uint64_t, 5> reset{arguments.base_pointer(), arguments.size_pointer(),
                                             X_MEM_RESET, X_PAGE_READWRITE, 0U};
    call = runtime.Execute(allocate, reset);
    Require(call.failure.error == RuntimeError::ImportServiceRefused &&
                call.failure.detail.find("unsupported service") != std::string::npos,
            "MEM_RESET did not refuse with its typed reason");

    const std::array<std::uint64_t, 5> unmapped{UINT32_MAX, UINT32_MAX, X_MEM_COMMIT,
                                                X_PAGE_READWRITE, 0U};
    call = runtime.Execute(allocate, unmapped);
    Require(call.failure.error == RuntimeError::ImportServiceRefused &&
                call.failure.detail.find("invalid guest memory") != std::string::npos,
            "an unmapped in/out pointer did not stop translated guest execution");

    const std::array<std::uint64_t, 3> bad_region_type{address, info_address, 3U};
    call = runtime.Execute(query, bad_region_type);
    Require(static_cast<bool>(call) && call.value == X_STATUS_INVALID_PARAMETER,
            "an unknown region type was not refused");

    std::cout << "kernel memory contract: allocate, query, decommit, release, and their refusal "
                 "paths crossed Xenia's export machinery and its own heaps\n";
    return 0;
}
