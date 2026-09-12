#include "executable_invalidation.hpp"

#include "guest_execution_budget.hpp"

#include <algorithm>
#include <unordered_set>

#include "xenia/cpu/function.h"
#include "xenia/cpu/processor.h"
#include "xenia/memory.h"

namespace x360port
{
namespace
{

[[nodiscard]] RuntimeFailure Failure(RuntimeError error, const char* detail)
{
    return RuntimeFailure{error, detail};
}

} // namespace

ExecutableInvalidation::~ExecutableInvalidation() { Disarm(); }

RuntimeFailure ExecutableInvalidation::Arm(xe::Memory& memory, CodeRange code_range)
{
    if (code_range.base == 0U || code_range.size == 0U)
    {
        return Failure(RuntimeError::ExecutableWatchRegistrationFailed,
                       "automatic executable-write observation requires a nonempty code range");
    }

    Disarm();
    memory_ = &memory;
    code_range_ = code_range;
    callback_handle_ = memory_->RegisterVirtualMemoryInvalidationCallback(OnVirtualWrite, this);
    if (callback_handle_ == nullptr)
    {
        memory_ = nullptr;
        code_range_ = {};
        return Failure(RuntimeError::ExecutableWatchRegistrationFailed,
                       "Xenia refused the automatic executable-write callback");
    }
    if (!Rearm())
    {
        Disarm();
        return Failure(RuntimeError::ExecutableWatchRegistrationFailed,
                       "Xenia refused the authenticated executable watch range");
    }
    return {};
}

RuntimeFailure ExecutableInvalidation::DrainObservedWrites()
{
    const std::uint64_t packed =
        pending_write_.exchange(kNoPendingWrite, std::memory_order_acq_rel);
    const std::uint32_t virtual_start = static_cast<std::uint32_t>(packed >> 32);
    const std::uint32_t virtual_last = static_cast<std::uint32_t>(packed);
    if (virtual_start != UINT32_MAX && virtual_start <= virtual_last)
    {
        const std::uint64_t watch_end =
            static_cast<std::uint64_t>(code_range_.base) + code_range_.size;
        const std::uint64_t write_end = static_cast<std::uint64_t>(virtual_last) + 1U;
        const std::uint64_t start = std::max<std::uint64_t>(virtual_start, code_range_.base);
        const std::uint64_t end = std::min(write_end, watch_end);
        if (start < end)
        {
            const std::uint32_t length = static_cast<std::uint32_t>(end - start);
            if (const RuntimeFailure failure =
                    Notify(code_range_, static_cast<GuestAddress>(start), length))
            {
                return failure;
            }
        }
    }
    if (!Rearm())
    {
        return Failure(RuntimeError::ExecutableWatchRearmFailed,
                       "Xenia refused to rearm the authenticated executable watch range");
    }
    return {};
}

void ExecutableInvalidation::OnVirtualWrite(void* context, std::uint32_t virtual_address,
                                            std::uint32_t length)
{
    auto& invalidation = *static_cast<ExecutableInvalidation*>(context);
    invalidation.RecordVirtualWrite(virtual_address, length);
}

void ExecutableInvalidation::RecordVirtualWrite(std::uint32_t virtual_address,
                                                std::uint32_t length) noexcept
{
    if (length == 0U || memory_ == nullptr)
    {
        return;
    }
    const std::uint64_t watch_end = static_cast<std::uint64_t>(code_range_.base) + code_range_.size;
    const std::uint64_t write_end = static_cast<std::uint64_t>(virtual_address) + length;
    const std::uint64_t start = std::max<std::uint64_t>(virtual_address, code_range_.base);
    const std::uint64_t end = std::min(write_end, watch_end);
    if (start >= end)
    {
        return;
    }

    const std::uint32_t first = static_cast<std::uint32_t>(start);
    const std::uint32_t last = static_cast<std::uint32_t>(end - 1U);
    std::uint64_t current = pending_write_.load(std::memory_order_relaxed);
    for (;;)
    {
        const std::uint32_t current_first = static_cast<std::uint32_t>(current >> 32);
        const std::uint32_t current_last = static_cast<std::uint32_t>(current);
        const std::uint32_t merged_first = std::min(current_first, first);
        const std::uint32_t merged_last = std::max(current_last, last);
        const std::uint64_t merged = (static_cast<std::uint64_t>(merged_first) << 32) | merged_last;
        if (pending_write_.compare_exchange_weak(current, merged, std::memory_order_release,
                                                 std::memory_order_relaxed))
        {
            MarkGuestExecutableWriteObserved();
            ++statistics_.observed_executable_writes;
            return;
        }
    }
}

bool ExecutableInvalidation::Rearm() noexcept
{
    if (memory_ != nullptr && callback_handle_ != nullptr && code_range_.size != 0U)
    {
        return memory_->EnableVirtualMemoryAccessCallbacks(callback_handle_, code_range_.base,
                                                           code_range_.size);
    }
    return false;
}

void ExecutableInvalidation::Disarm() noexcept
{
    if (memory_ != nullptr && callback_handle_ != nullptr)
    {
        memory_->UnregisterVirtualMemoryInvalidationCallback(callback_handle_);
    }
    callback_handle_ = nullptr;
    memory_ = nullptr;
    code_range_ = {};
    pending_write_.store(kNoPendingWrite, std::memory_order_release);
}

RuntimeFailure ExecutableInvalidation::Notify(CodeRange code_range, GuestAddress address,
                                              std::uint32_t size)
{
    const std::uint64_t code_end = static_cast<std::uint64_t>(code_range.base) + code_range.size;
    const std::uint64_t write_end = static_cast<std::uint64_t>(address) + size;
    if (size == 0 || address < code_range.base || write_end > code_end)
    {
        return Failure(
            RuntimeError::ExecutableRangeInvalid,
            "executable write must be non-empty and inside the authenticated code range");
    }

    std::unordered_set<GuestAddress> invalidated;
    const GuestAddress first_instruction = address & ~GuestAddress{3};
    for (std::uint64_t instruction = first_instruction; instruction < write_end; instruction += 4)
    {
        for (xe::cpu::Function* function :
             processor_.FindFunctionsWithAddress(static_cast<GuestAddress>(instruction)))
        {
            if (invalidated.insert(function->address()).second)
            {
                processor_.RemoveFunctionByAddress(function->address());
                ++statistics_.translation_invalidations;
            }
        }
    }
    return {};
}

} // namespace x360port
