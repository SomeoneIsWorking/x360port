#include "guest_virtual_memory.hpp"

#include "guest_endian.hpp"

#include "xenia/base/math.h"
#include "xenia/kernel/kernel.h"
#include "xenia/kernel/xboxkrnl/xboxkrnl_memory.h"
#include "xenia/memory.h"
#include "xenia/xbox.h"

#include <cstddef>

namespace x360port
{
namespace
{

using xe::X_STATUS;
using xe::kernel::X_MEM_COMMIT;
using xe::kernel::X_MEM_DECOMMIT;
using xe::kernel::X_MEM_FREE;
using xe::kernel::X_MEM_LARGE_PAGES;
using xe::kernel::X_MEM_NOZERO;
using xe::kernel::X_MEM_PRIVATE;
using xe::kernel::X_MEM_RELEASE;
using xe::kernel::X_MEM_RESERVE;
using xe::kernel::X_MEM_RESET;
using xe::kernel::X_MEM_TOP_DOWN;

constexpr std::uint32_t kSmallPageSize = 4U * 1024U;
constexpr std::uint32_t kLargePageSize = 64U * 1024U;
constexpr std::size_t kMemoryBasicInformationBytes = 28U;

// One in/out guest word, which is what every pointer parameter of these
// exports carries. Binding the address to the call keeps the two apart at each
// use instead of passing a pair of interchangeable integers.
class GuestWordSlot final
{
  public:
    GuestWordSlot(const GuestImportContext& call, GuestAddress address) noexcept
        : call_(&call), address_(address)
    {
    }

    [[nodiscard]] bool present() const noexcept { return address_ != 0U; }

    [[nodiscard]] bool Read(std::uint32_t& value) const noexcept
    {
        GuestWord bytes{};
        if (!call_->read_memory(address_, bytes))
        {
            return false;
        }
        value = FromGuestWord(bytes);
        return true;
    }

    [[nodiscard]] bool Write(std::uint32_t value) const noexcept
    {
        return call_->write_memory(address_, ToGuestWord(value));
    }

  private:
    const GuestImportContext* call_;
    GuestAddress address_;
};

// The region a caller asked for, before page rounding.
struct RequestedRegion final
{
    std::uint32_t base = 0;
    std::uint32_t size = 0;
};

// How the caller asked for that region to be mapped.
struct AllocationPolicy final
{
    std::uint32_t alloc_type = 0;
    std::uint32_t protect = 0;
};

// A page-aligned range the allocator has already handed back.
struct CommittedRange final
{
    std::uint32_t address = 0;
    std::uint32_t size = 0;
};

// Titles are known to pass a signed-negative region size; the console takes its
// magnitude rather than rounding up a value near 4 GiB.
[[nodiscard]] std::uint32_t NormalizeRequestedSize(std::uint32_t size) noexcept
{
    const auto signed_size = static_cast<std::int32_t>(size);
    return signed_size < 0 ? static_cast<std::uint32_t>(-signed_size) : size;
}

[[nodiscard]] std::uint32_t AllocationFlags(std::uint32_t alloc_type) noexcept
{
    std::uint32_t flags = 0;
    if ((alloc_type & X_MEM_RESERVE) != 0U)
    {
        flags |= xe::kMemoryAllocationReserve;
    }
    if ((alloc_type & X_MEM_COMMIT) != 0U)
    {
        flags |= xe::kMemoryAllocationCommit;
    }
    return flags;
}

// A commit must observe zeroed pages, which requires write access even when the
// caller asked for a read-only mapping; the requested protection is restored
// before the allocation is handed back.
void ZeroNewlyCommittedPages(xe::Memory& memory, xe::BaseHeap& heap, CommittedRange range,
                             AllocationPolicy policy) noexcept
{
    if ((policy.alloc_type & X_MEM_NOZERO) != 0U || (policy.alloc_type & X_MEM_COMMIT) == 0U)
    {
        return;
    }
    const bool writable = (policy.protect & xe::kMemoryProtectWrite) != 0U;
    if (!writable)
    {
        heap.Protect(range.address, range.size, xe::kMemoryProtectRead | xe::kMemoryProtectWrite);
    }
    memory.Zero(range.address, range.size);
    if (!writable)
    {
        heap.Protect(range.address, range.size, policy.protect);
    }
}

[[nodiscard]] std::uint32_t GuestAllocationState(std::uint32_t state) noexcept
{
    if ((state & xe::kMemoryAllocationCommit) != 0U)
    {
        return X_MEM_COMMIT;
    }
    if ((state & xe::kMemoryAllocationReserve) != 0U)
    {
        return X_MEM_RESERVE;
    }
    return X_MEM_FREE;
}

[[nodiscard]] std::array<std::byte, kMemoryBasicInformationBytes>
EncodeRegionInfo(const xe::HeapAllocationInfo& info) noexcept
{
    std::array<std::byte, kMemoryBasicInformationBytes> bytes{};
    StoreBe32(bytes, 0U, info.base_address);
    StoreBe32(bytes, 4U, info.allocation_base);
    StoreBe32(bytes, 8U, xe::kernel::xboxkrnl::ToXdkProtectFlags(info.allocation_protect));
    StoreBe32(bytes, 12U, info.region_size);
    StoreBe32(bytes, 16U, GuestAllocationState(info.state));
    StoreBe32(bytes, 20U, xe::kernel::xboxkrnl::ToXdkProtectFlags(info.protect));
    StoreBe32(bytes, 24U, X_MEM_PRIVATE);
    return bytes;
}

// The heap that owns a guest pointer is decided by its address, not by the
// export the title happened to call: a title's own free wrapper routes both
// physical and virtual resources through the same call site.
[[nodiscard]] xe::BaseHeap* LookupVirtualHeap(xe::Memory& memory, std::uint32_t address) noexcept
{
    xe::BaseHeap* heap = memory.LookupHeap(address);
    if (heap == nullptr || heap->heap_type() != xe::HeapType::kGuestVirtual)
    {
        return nullptr;
    }
    return heap;
}

struct AllocationRequest final
{
    std::uint32_t base = 0;
    std::uint32_t size = 0;
    std::uint32_t page_size = 0;
};

[[nodiscard]] bool PrepareRequest(xe::Memory& memory, RequestedRegion requested,
                                  std::uint32_t alloc_type, AllocationRequest& request) noexcept
{
    if (requested.base != 0U)
    {
        // A title probes the XPS and MMIO ranges through this export, and those
        // are not guest-virtual heaps; refusing here is the console's answer.
        const xe::BaseHeap* heap = LookupVirtualHeap(memory, requested.base);
        if (heap == nullptr)
        {
            return false;
        }
        request.page_size = heap->page_size();
    }
    else
    {
        request.page_size =
            (alloc_type & X_MEM_LARGE_PAGES) != 0U ? kLargePageSize : kSmallPageSize;
    }
    request.base = requested.base - (requested.base % request.page_size);
    request.size = xe::round_up(NormalizeRequestedSize(requested.size),
                                request.base != 0U ? request.page_size : kLargePageSize);
    return request.size != 0U;
}

struct Allocation final
{
    xe::BaseHeap* heap = nullptr;
    std::uint32_t address = 0;
    bool was_committed = false;
    bool page_size_conflict = false;
};

[[nodiscard]] Allocation PerformAllocation(xe::Memory& memory, const AllocationRequest& request,
                                           AllocationPolicy policy) noexcept
{
    Allocation allocation{};
    const std::uint32_t flags = AllocationFlags(policy.alloc_type);
    if (request.base != 0U)
    {
        allocation.heap = memory.LookupHeap(request.base);
        if (allocation.heap == nullptr)
        {
            return allocation;
        }
        if (allocation.heap->page_size() != request.page_size)
        {
            allocation.page_size_conflict = true;
            return allocation;
        }
        xe::HeapAllocationInfo info{};
        allocation.was_committed = allocation.heap->QueryRegionInfo(request.base, &info) &&
                                   (info.state & xe::kMemoryAllocationCommit) != 0U;
        if (allocation.heap->AllocFixed(request.base, request.size, request.page_size, flags,
                                        policy.protect))
        {
            allocation.address = request.base;
        }
        return allocation;
    }
    allocation.heap = memory.LookupHeapByType(false, request.page_size);
    if (allocation.heap == nullptr)
    {
        return allocation;
    }
    const bool top_down = (policy.alloc_type & X_MEM_TOP_DOWN) != 0U;
    allocation.heap->Alloc(request.size, request.page_size, flags, policy.protect, top_down,
                           &allocation.address);
    return allocation;
}

void AllocateVirtualMemory(xe::Memory& memory, GuestImportContext& call) noexcept
{
    const auto base_pointer = static_cast<GuestAddress>(call.argument(0));
    const auto size_pointer = static_cast<GuestAddress>(call.argument(1));
    const auto alloc_type = static_cast<std::uint32_t>(call.argument(2));
    const auto protect_bits = static_cast<std::uint32_t>(call.argument(3));
    if (base_pointer == 0U || size_pointer == 0U)
    {
        call.set_return_value(X_STATUS_INVALID_PARAMETER);
        return;
    }
    const GuestWordSlot base_slot(call, base_pointer);
    const GuestWordSlot size_slot(call, size_pointer);
    RequestedRegion requested{};
    if (!base_slot.Read(requested.base) || !size_slot.Read(requested.size))
    {
        call.refuse(ImportRefusalReason::InvalidGuestMemory);
        return;
    }
    if (requested.size == 0U || (alloc_type & (X_MEM_COMMIT | X_MEM_RESERVE | X_MEM_RESET)) == 0U ||
        ((alloc_type & X_MEM_RESET) != 0U && (alloc_type & ~X_MEM_RESET) != 0U))
    {
        call.set_return_value(X_STATUS_INVALID_PARAMETER);
        return;
    }
    if ((alloc_type & X_MEM_RESET) != 0U)
    {
        // MEM_RESET discards page contents while keeping the mapping. Nothing
        // below implements that, so the call refuses by name instead of
        // reporting a success the title's pages would not reflect.
        call.refuse(ImportRefusalReason::UnsupportedService);
        return;
    }

    AllocationRequest request{};
    if (!PrepareRequest(memory, requested, alloc_type, request))
    {
        call.set_return_value(X_STATUS_INVALID_PARAMETER);
        return;
    }
    const AllocationPolicy policy{.alloc_type = alloc_type,
                                  .protect =
                                      xe::kernel::xboxkrnl::FromXdkProtectFlags(protect_bits)};
    const Allocation allocation = PerformAllocation(memory, request, policy);
    if (allocation.page_size_conflict)
    {
        call.set_return_value(X_STATUS_ACCESS_DENIED);
        return;
    }
    if (allocation.address == 0U || allocation.heap == nullptr)
    {
        call.set_return_value(X_STATUS_NO_MEMORY);
        return;
    }
    if (!allocation.was_committed)
    {
        ZeroNewlyCommittedPages(memory, *allocation.heap,
                                CommittedRange{allocation.address, request.size}, policy);
    }
    if (!base_slot.Write(allocation.address) || !size_slot.Write(request.size))
    {
        call.refuse(ImportRefusalReason::InvalidGuestMemory);
        return;
    }
    call.set_return_value(X_STATUS_SUCCESS);
}

void FreeVirtualMemory(xe::Memory& memory, GuestImportContext& call) noexcept
{
    const auto base_pointer = static_cast<GuestAddress>(call.argument(0));
    const auto size_pointer = static_cast<GuestAddress>(call.argument(1));
    const auto free_type = static_cast<std::uint32_t>(call.argument(2));
    if (base_pointer == 0U)
    {
        call.set_return_value(X_STATUS_INVALID_PARAMETER);
        return;
    }
    const GuestWordSlot base_slot(call, base_pointer);
    const GuestWordSlot size_slot(call, size_pointer);
    std::uint32_t address = 0;
    if (!base_slot.Read(address))
    {
        call.refuse(ImportRefusalReason::InvalidGuestMemory);
        return;
    }
    // A free of address zero is ordinary traffic, not a defect: a title's own
    // free wrapper forwards its argument unchecked, and a resource destructor
    // calls it on a pointer it never filled in. The console answers without
    // consulting a heap, so this must not present as an allocator failure.
    if (address == 0U)
    {
        call.set_return_value(X_STATUS_MEMORY_NOT_ALLOCATED);
        return;
    }
    xe::BaseHeap* heap = LookupVirtualHeap(memory, address);
    if (heap == nullptr)
    {
        call.set_return_value(X_STATUS_INVALID_PARAMETER);
        return;
    }

    std::uint32_t size = 0;
    // MEM_DECOMMIT keeps the reservation, so the guest still owns the address
    // range; only MEM_RELEASE returns it. A FreeType of zero is not documented
    // and is treated as a release, which is what the recovered handler did.
    const bool decommit_only =
        (free_type & X_MEM_DECOMMIT) != 0U && (free_type & X_MEM_RELEASE) == 0U;
    if (decommit_only)
    {
        if (!size_slot.present() || !size_slot.Read(size))
        {
            call.set_return_value(X_STATUS_INVALID_PARAMETER);
            return;
        }
        size = xe::round_up(size, heap->page_size());
        if (size == 0U || !heap->Decommit(address, size))
        {
            call.set_return_value(X_STATUS_UNSUCCESSFUL);
            return;
        }
    }
    else if (!heap->Release(address, &size))
    {
        // A failed release is reported rather than swallowed: answering success
        // would tell the title's wrapper that a free it never performed worked.
        call.set_return_value(X_STATUS_UNSUCCESSFUL);
        return;
    }

    if (!base_slot.Write(address) || (size_slot.present() && !size_slot.Write(size)))
    {
        call.refuse(ImportRefusalReason::InvalidGuestMemory);
        return;
    }
    call.set_return_value(X_STATUS_SUCCESS);
}

void QueryVirtualMemory(xe::Memory& memory, GuestImportContext& call) noexcept
{
    const auto address = static_cast<std::uint32_t>(call.argument(0));
    const auto information_pointer = static_cast<GuestAddress>(call.argument(1));
    const auto region_type = static_cast<std::uint32_t>(call.argument(2));
    if (information_pointer == 0U || region_type > 2U)
    {
        call.set_return_value(X_STATUS_INVALID_PARAMETER);
        return;
    }
    xe::BaseHeap* heap = memory.LookupHeap(address);
    xe::HeapAllocationInfo info{};
    if (heap == nullptr || !heap->QueryRegionInfo(address, &info))
    {
        call.set_return_value(X_STATUS_INVALID_PARAMETER);
        return;
    }
    if (!call.write_memory(information_pointer, EncodeRegionInfo(info)))
    {
        call.refuse(ImportRefusalReason::InvalidGuestMemory);
        return;
    }
    call.set_return_value(X_STATUS_SUCCESS);
}

} // namespace

std::array<ImportClaim, 3> GuestVirtualMemory::Claims() noexcept
{
    return {ImportClaim{.library = ExportNames::Library::Kernel,
                        .export_name = "NtAllocateVirtualMemory",
                        .handler = Allocate,
                        .context = this},
            ImportClaim{.library = ExportNames::Library::Kernel,
                        .export_name = "NtFreeVirtualMemory",
                        .handler = Free,
                        .context = this},
            ImportClaim{.library = ExportNames::Library::Kernel,
                        .export_name = "NtQueryVirtualMemory",
                        .handler = Query,
                        .context = this}};
}

void GuestVirtualMemory::Allocate(GuestImportContext& call, void* service) noexcept
{
    AllocateVirtualMemory(*static_cast<GuestVirtualMemory*>(service)->memory_, call);
}

void GuestVirtualMemory::Free(GuestImportContext& call, void* service) noexcept
{
    FreeVirtualMemory(*static_cast<GuestVirtualMemory*>(service)->memory_, call);
}

void GuestVirtualMemory::Query(GuestImportContext& call, void* service) noexcept
{
    QueryVirtualMemory(*static_cast<GuestVirtualMemory*>(service)->memory_, call);
}

} // namespace x360port
