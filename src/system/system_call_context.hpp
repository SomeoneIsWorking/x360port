#ifndef X360PORT_SYSTEM_CALL_CONTEXT_HPP
#define X360PORT_SYSTEM_CALL_CONTEXT_HPP

#include "../guest_memory.hpp"
#include "x360port/guest_call.hpp"

namespace xe
{
class Memory;
namespace cpu
{
class Processor;
}
} // namespace xe

namespace x360port
{

// The call context overrides receive inside a full system. It holds no
// per-call state: the original body runs on whichever guest thread entered the
// override, found through Xenia's thread-local ThreadState, so one instance
// serves every guest thread at once.
class SystemCallContext final : public GuestCallContext
{
  public:
    SystemCallContext(xe::Memory& memory, xe::cpu::Processor& processor) noexcept;
    ~SystemCallContext() = default;

    [[nodiscard]] RuntimeFailure ReadMappedGuestMemory(GuestAddress address,
                                                       std::span<std::byte> bytes) const override;
    [[nodiscard]] RuntimeFailure WriteMappedGuestMemory(GuestAddress address,
                                                        std::span<const std::byte> bytes) override;
    [[nodiscard]] ExecutionResult
    CallOriginalBody(GuestAddress address, std::span<const std::uint64_t> arguments) override;

  private:
    GuestMemory memory_;
    xe::cpu::Processor* processor_;
};

} // namespace x360port

#endif
