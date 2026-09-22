#ifndef X360PORT_GUEST_FAULT_GUARD_HPP
#define X360PORT_GUEST_FAULT_GUARD_HPP

#include <cstdint>

namespace xe
{
class Exception;
class Memory;
} // namespace xe

namespace xe::cpu::backend
{
class CodeCache;
} // namespace xe::cpu::backend

namespace x360port
{

// Terminates a guest call whose translated code faults on guest memory.
//
// The translated-block budget cannot bound such a call: it is decremented at
// block entry, so an instruction that faults repeatedly never reaches another
// boundary. Xenia's handler chain declines an access violation that is neither
// MMIO nor a watched page, and the signal handler then returns to the faulting
// instruction, which faults again forever.
//
// This guard installs last in that chain and diverts the host program counter
// to the faulting function's own epilogue, the same exit a budget exhaustion
// takes, so the call unwinds through Xenia's frames and reports a typed
// failure. It only ever claims a fault raised inside generated code while a
// guest call is active on this thread; anything else is left to the host.
class GuestFaultGuard final
{
  public:
    GuestFaultGuard(xe::cpu::backend::CodeCache& code_cache, xe::Memory& memory) noexcept;
    ~GuestFaultGuard();

    GuestFaultGuard(const GuestFaultGuard&) = delete;
    GuestFaultGuard& operator=(const GuestFaultGuard&) = delete;
    GuestFaultGuard(GuestFaultGuard&&) = delete;
    GuestFaultGuard& operator=(GuestFaultGuard&&) = delete;

  private:
    static bool Dispatch(xe::Exception* exception, void* context);
    [[nodiscard]] bool Claim(xe::Exception& exception) noexcept;
    [[nodiscard]] bool InGeneratedCode(std::uint64_t host_pc) const noexcept;

    xe::cpu::backend::CodeCache& code_cache_;
    xe::Memory& memory_;
};

} // namespace x360port

#endif
