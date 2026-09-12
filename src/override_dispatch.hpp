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

    void Bind(xe::cpu::Processor& processor, RuntimeContext& owner,
              JitStatistics& statistics) noexcept;

    [[nodiscard]] RuntimeFailure Install(GuestAddress address, NativeOverrideHandler handler,
                                         void* handler_context, CodeRange code_range);
    [[nodiscard]] RuntimeFailure Remove(GuestAddress address);
    [[nodiscard]] std::optional<Entry> Find(GuestAddress address) const noexcept;

  private:
    static void DispatchGuest(xe::cpu::ppc::PPCContext_s* context, void* raw_entry, void*) noexcept;

    xe::cpu::Processor* processor_ = nullptr;
    RuntimeContext* owner_ = nullptr;
    JitStatistics* statistics_ = nullptr;
    std::unordered_map<GuestAddress, Entry> entries_;
};

} // namespace x360port

#endif
