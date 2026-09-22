#ifndef X360PORT_GUEST_THREAD_CONTEXT_HPP
#define X360PORT_GUEST_THREAD_CONTEXT_HPP

#include "x360port/runtime.hpp"

#include <cstdint>
#include <memory>

namespace xe
{
class Memory;
namespace cpu
{
class Processor;
class ThreadState;
} // namespace cpu
} // namespace xe

namespace x360port
{

// Owns the single guest thread x360port executes on: its bounded call stack,
// its processor control region, and the Xenia ThreadState that binds the two.
// The stack and PCR are guest allocations whose lifetime is exactly this
// object's, so the runtime does not free them from its own teardown.
class GuestThreadContext final
{
  public:
    GuestThreadContext() = default;
    GuestThreadContext(const GuestThreadContext&) = delete;
    GuestThreadContext& operator=(const GuestThreadContext&) = delete;
    GuestThreadContext(GuestThreadContext&&) = delete;
    GuestThreadContext& operator=(GuestThreadContext&&) = delete;

    ~GuestThreadContext();

    [[nodiscard]] RuntimeFailure Initialize(xe::Memory& memory, xe::cpu::Processor& processor);
    // Releases the thread state before the guest allocations it addresses. The
    // runtime calls this explicitly so teardown stays ordered against the
    // processor whose code cache the thread ran in.
    void Reset() noexcept;

    [[nodiscard]] bool initialized() const noexcept { return state_ != nullptr; }
    // Valid only once Initialize has succeeded; the runtime never executes
    // before that.
    [[nodiscard]] xe::cpu::ThreadState& State() const noexcept { return *state_; }

  private:
    static constexpr std::uint32_t kThreadId = 0x360;
    static constexpr std::uint32_t kStackSize = 64U * 1024U;
    static constexpr std::uint32_t kPcrSize = 0x1000U;

    xe::Memory* memory_ = nullptr;
    std::unique_ptr<xe::cpu::ThreadState> state_;
    std::uint32_t stack_address_ = 0;
    std::uint32_t pcr_address_ = 0;
};

} // namespace x360port

#endif // X360PORT_GUEST_THREAD_CONTEXT_HPP
