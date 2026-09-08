#ifndef X360PORT_OVERRIDE_DISPATCH_HPP
#define X360PORT_OVERRIDE_DISPATCH_HPP

#include "x360port/runtime.hpp"

#include <optional>
#include <unordered_map>

namespace x360port
{

class OverrideDispatch final
{
  public:
    struct Entry
    {
        NativeOverrideHandler handler = nullptr;
        void* context = nullptr;
    };

    using InvalidateFunction = void (*)(void* context, GuestAddress address) noexcept;

    [[nodiscard]] RuntimeFailure Install(GuestAddress address, NativeOverrideHandler handler,
                                         void* handler_context, CodeRange code_range,
                                         InvalidateFunction invalidate, void* invalidate_context);
    [[nodiscard]] RuntimeFailure Remove(GuestAddress address, InvalidateFunction invalidate,
                                        void* invalidate_context);
    [[nodiscard]] std::optional<Entry> Find(GuestAddress address) const noexcept;

  private:
    std::unordered_map<GuestAddress, Entry> entries_;
};

} // namespace x360port

#endif
