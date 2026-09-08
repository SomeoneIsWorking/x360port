#include "override_dispatch.hpp"

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

RuntimeFailure OverrideDispatch::Install(GuestAddress address, NativeOverrideHandler handler,
                                         void* handler_context, CodeRange code_range,
                                         InvalidateFunction invalidate, void* invalidate_context)
{
    if (handler == nullptr)
    {
        return Failure(RuntimeError::OverrideInvalid, "native override handler must not be null");
    }
    if (!Contains(code_range, address))
    {
        return Failure(RuntimeError::EntryOutsideCode,
                       "native override address is outside the authenticated code range");
    }
    if (entries_.contains(address))
    {
        return Failure(RuntimeError::OverrideAlreadyInstalled,
                       "a native override is already installed at this guest address");
    }

    entries_.emplace(address, Entry{handler, handler_context});
    invalidate(invalidate_context, address);
    return {};
}

RuntimeFailure OverrideDispatch::Remove(GuestAddress address, InvalidateFunction invalidate,
                                        void* invalidate_context)
{
    if (!entries_.erase(address))
    {
        return Failure(RuntimeError::OverrideNotInstalled,
                       "no native override is installed at this guest address");
    }

    invalidate(invalidate_context, address);
    return {};
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
