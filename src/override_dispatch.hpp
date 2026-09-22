#ifndef X360PORT_OVERRIDE_DISPATCH_HPP
#define X360PORT_OVERRIDE_DISPATCH_HPP

#include "x360port/runtime.hpp"

#include <optional>
#include <unordered_map>

namespace xe::cpu
{
class Processor;
namespace ppc
{
struct PPCContext_s;
}
} // namespace xe::cpu

namespace x360port
{

class OverrideDispatch final
{
  public:
    struct Entry
    {
        GuestAddress address = 0;
        NativeOverrideHandler handler = nullptr;
        void* context = nullptr;
        OverrideDispatch* dispatch = nullptr;
    };

    // Receives a handler's failure when no bounded host call is active to
    // carry it back, as on a free-running guest thread of a full system. It
    // must not return: the guest state after a failed override is unknown.
    using UnscopedFailureSink = void (*)(const RuntimeFailure& failure) noexcept;

    void Bind(xe::cpu::Processor& processor, GuestCallContext& owner, JitStatistics& statistics,
              UnscopedFailureSink unscoped_failure = nullptr) noexcept;

    [[nodiscard]] RuntimeFailure Install(GuestAddress address, NativeOverrideHandler handler,
                                         void* handler_context, CodeRange code_range);
    [[nodiscard]] RuntimeFailure Remove(GuestAddress address);
    [[nodiscard]] std::optional<Entry> Find(GuestAddress address) const noexcept;

  private:
    static void DispatchGuest(xe::cpu::ppc::PPCContext_s* context, void* raw_entry, void*) noexcept;

    xe::cpu::Processor* processor_ = nullptr;
    GuestCallContext* owner_ = nullptr;
    UnscopedFailureSink unscoped_failure_ = nullptr;
    JitStatistics* statistics_ = nullptr;
    std::unordered_map<GuestAddress, Entry> entries_;
};

} // namespace x360port

#endif
