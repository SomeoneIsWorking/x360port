#include "override_dispatch.hpp"

#include "guest_execution_budget.hpp"

#include <array>
#include <cstdint>
#include <exception>
#include <utility>

#include "xenia/cpu/ppc/ppc_context.h"
#include "xenia/cpu/processor.h"

namespace x360port
{
namespace
{

[[nodiscard]] RuntimeFailure Failure(RuntimeError error, const char* detail)
{
    return RuntimeFailure{error, detail};
}

[[nodiscard]] bool Contains(CodeRange range, GuestAddress address) noexcept
{
    return address >= range.base && static_cast<std::uint64_t>(address) <
                                        static_cast<std::uint64_t>(range.base) + range.size;
}

} // namespace

void OverrideDispatch::Bind(xe::cpu::Processor& processor, RuntimeContext& owner,
                            JitStatistics& statistics) noexcept
{
    processor_ = &processor;
    owner_ = &owner;
    statistics_ = &statistics;
}

RuntimeFailure OverrideDispatch::Install(GuestAddress address, NativeOverrideHandler handler,
                                         void* handler_context, CodeRange code_range)
{
    if (processor_ == nullptr || owner_ == nullptr || statistics_ == nullptr)
    {
        return Failure(RuntimeError::LoadStateInvalid,
                       "native override dispatch requires an initialized runtime");
    }
    if (handler == nullptr)
    {
        return Failure(RuntimeError::OverrideInvalid, "native override handler must not be null");
    }
    if (!Contains(code_range, address))
    {
        return Failure(RuntimeError::EntryOutsideCode,
                       "native override address is outside the authenticated code range");
    }
    auto [entry, inserted] =
        entries_.emplace(address, Entry{address, handler, handler_context, this});
    if (!inserted)
    {
        return Failure(RuntimeError::OverrideAlreadyInstalled,
                       "a native override is already installed at this guest address");
    }
    if (!processor_->InstallGuestCallRedirect(address, DispatchGuest, &entry->second, nullptr))
    {
        entries_.erase(address);
        return Failure(RuntimeError::OverrideDispatchFailed,
                       "Xenia could not install the native guest-call redirect");
    }
    return {};
}

RuntimeFailure OverrideDispatch::Remove(GuestAddress address)
{
    if (!entries_.contains(address))
    {
        return Failure(RuntimeError::OverrideNotInstalled,
                       "no native override is installed at this guest address");
    }
    if (!processor_->RemoveGuestCallRedirect(address))
    {
        return Failure(RuntimeError::OverrideDispatchFailed,
                       "Xenia could not remove the native guest-call redirect");
    }
    entries_.erase(address);
    return {};
}

void OverrideDispatch::DispatchGuest(xe::cpu::ppc::PPCContext_s* context, void* raw_entry,
                                     void*) noexcept
{
    const auto& entry = *static_cast<Entry*>(raw_entry);
    auto& self = *entry.dispatch;
    if (self.owner_ == nullptr || self.statistics_ == nullptr)
    {
        std::terminate();
    }
    std::array<std::uint64_t, 8> arguments{};
    for (std::size_t index = 0; index < arguments.size(); ++index)
    {
        arguments[index] = context->r[3U + index];
    }
    ++self.statistics_->native_override_calls;
    ExecutionResult result = entry.handler(*self.owner_, entry.address, arguments, entry.context);
    if (result)
    {
        context->r[3] = result.value;
    }
    else
    {
        MarkGuestOverrideFailed(std::move(result.failure));
    }
}

std::optional<OverrideDispatch::Entry> OverrideDispatch::Find(GuestAddress address) const noexcept
{
    const auto entry = entries_.find(address);
    if (entry == entries_.end())
    {
        return std::nullopt;
    }
    return entry->second;
}

} // namespace x360port
