#include "guest_memory.hpp"

#include <cstring>
#include <iterator>
#include <limits>
#include <utility>

#include "xenia/memory.h"

namespace x360port
{
namespace
{

[[nodiscard]] RuntimeFailure Failure(RuntimeError error, std::string detail)
{
    return RuntimeFailure{error, std::move(detail)};
}

} // namespace

GuestMemory::~GuestMemory() { Reset(); }

void GuestMemory::Reset() noexcept
{
    if (memory_ != nullptr)
    {
        for (const auto& [address, size] : allocations_)
        {
            static_cast<void>(memory_->SystemHeapFree(address));
            static_cast<void>(size);
        }
    }
    allocations_.clear();
    memory_ = nullptr;
}

void GuestMemory::Initialize(xe::Memory& memory) noexcept { memory_ = &memory; }

GuestMemoryAllocationResult GuestMemory::Allocate(std::uint32_t size)
{
    if (memory_ == nullptr || size == 0U)
    {
        return {
            {},
            Failure(RuntimeError::GuestMemoryAllocationFailed,
                    "guest memory allocation requires an initialized runtime and nonzero size")};
    }

    const GuestAddress address = memory_->SystemHeapAlloc(size);
    if (address == 0U || allocations_.contains(address))
    {
        return {{},
                Failure(RuntimeError::GuestMemoryAllocationFailed,
                        "Xenia could not allocate a tracked guest-memory range")};
    }
    allocations_.emplace(address, size);
    return {GuestMemoryAllocation{address, size}, {}};
}

RuntimeFailure GuestMemory::Read(GuestAddress address, std::span<std::byte> bytes) const
{
    if (memory_ == nullptr || bytes.empty() || !Contains(address, bytes.size()))
    {
        return Failure(RuntimeError::GuestMemoryRangeInvalid,
                       "guest-memory read must stay inside one live allocation");
    }
    std::memcpy(bytes.data(), memory_->TranslateVirtual(address), bytes.size());
    return {};
}

RuntimeFailure GuestMemory::Write(GuestAddress address, std::span<const std::byte> bytes) const
{
    if (memory_ == nullptr || bytes.empty() || !Contains(address, bytes.size()))
    {
        return Failure(RuntimeError::GuestMemoryRangeInvalid,
                       "guest-memory write must stay inside one live allocation");
    }
    std::memcpy(memory_->TranslateVirtual(address), bytes.data(), bytes.size());
    return {};
}

RuntimeFailure GuestMemory::ReadMapped(GuestAddress address, std::span<std::byte> bytes) const
{
    if (!CanAccess(address, bytes.size(), false))
    {
        return Failure(RuntimeError::GuestMemoryRangeInvalid,
                       "mapped guest-memory read must stay inside one readable guest range");
    }
    std::memcpy(bytes.data(), memory_->TranslateVirtual(address), bytes.size());
    return {};
}

RuntimeFailure GuestMemory::WriteMapped(GuestAddress address,
                                        std::span<const std::byte> bytes) const
{
    if (!CanAccess(address, bytes.size(), true))
    {
        return Failure(RuntimeError::GuestMemoryRangeInvalid,
                       "mapped guest-memory write must stay inside one writable guest range");
    }
    std::memcpy(memory_->TranslateVirtual(address), bytes.data(), bytes.size());
    return {};
}

RuntimeFailure GuestMemory::Release(GuestMemoryAllocation allocation)
{
    if (memory_ == nullptr || allocation.address == 0U || allocation.size == 0U)
    {
        return Failure(RuntimeError::GuestMemoryRangeInvalid,
                       "guest-memory release requires a live allocation");
    }
    const auto it = allocations_.find(allocation.address);
    if (it == allocations_.end() || it->second != allocation.size)
    {
        return Failure(RuntimeError::GuestMemoryRangeInvalid,
                       "guest-memory release must match the complete allocation");
    }
    static_cast<void>(memory_->SystemHeapFree(allocation.address));
    allocations_.erase(it);
    return {};
}

bool GuestMemory::Contains(GuestAddress address, std::size_t size) const noexcept
{
    if (size == 0U || size > std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }
    const auto it = allocations_.upper_bound(address);
    if (it == allocations_.begin())
    {
        return false;
    }
    const auto allocation = std::prev(it);
    const std::uint64_t begin = allocation->first;
    const std::uint64_t end = begin + allocation->second;
    return address >= begin && static_cast<std::uint64_t>(address) + size <= end;
}

bool GuestMemory::CanAccess(GuestAddress address, std::size_t size, bool write) const noexcept
{
    if (memory_ == nullptr || size == 0U || size > std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }
    const std::uint64_t last_address = static_cast<std::uint64_t>(address) + size - 1U;
    if (last_address > std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }
    auto* first_heap = memory_->LookupHeap(address);
    auto* last_heap = memory_->LookupHeap(static_cast<GuestAddress>(last_address));
    if (first_heap == nullptr || first_heap != last_heap)
    {
        return false;
    }
    const auto access =
        first_heap->QueryRangeAccess(address, static_cast<GuestAddress>(last_address));
    const auto access_bits = static_cast<std::uint32_t>(access);
    const std::uint32_t required = write ? 0x2U : 0x1U;
    if ((access_bits & required) != required)
    {
        return false;
    }
    for (std::size_t offset = 0; offset < size; ++offset)
    {
        if (memory_->LookupVirtualMappedRange(address + static_cast<GuestAddress>(offset)) !=
            nullptr)
        {
            return false;
        }
    }
    return true;
}

} // namespace x360port
