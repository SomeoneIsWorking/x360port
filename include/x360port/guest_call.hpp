#ifndef X360PORT_GUEST_CALL_HPP
#define X360PORT_GUEST_CALL_HPP

#include "x360port/runtime_failure.hpp"
#include "x360port/validation.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace x360port
{

struct ExecutionResult
{
    RuntimeFailure failure;
    std::uint64_t value = 0;

    [[nodiscard]] explicit operator bool() const noexcept { return !failure; }
};

// What a native override may do while it stands in for a guest function: read
// and write the title's mapped memory, and re-enter the original guest body it
// replaced. Both the isolated leaf runtime and the full-system session provide
// one, so a title writes each override once and qualifies it against either.
class GuestCallContext
{
  public:
    GuestCallContext(const GuestCallContext&) = delete;
    GuestCallContext& operator=(const GuestCallContext&) = delete;
    GuestCallContext(GuestCallContext&&) = delete;
    GuestCallContext& operator=(GuestCallContext&&) = delete;

    // Require one committed, accessible guest range and reject device mappings.
    [[nodiscard]] virtual RuntimeFailure
    ReadMappedGuestMemory(GuestAddress address, std::span<std::byte> bytes) const = 0;
    [[nodiscard]] virtual RuntimeFailure
    WriteMappedGuestMemory(GuestAddress address, std::span<const std::byte> bytes) = 0;

    // Executes the translated original body at address on the calling guest
    // thread, bypassing the override installed there, and returns its r3.
    [[nodiscard]] virtual ExecutionResult
    CallOriginalBody(GuestAddress address, std::span<const std::uint64_t> arguments) = 0;

  protected:
    GuestCallContext() = default;
    ~GuestCallContext() = default;
};

using NativeOverrideHandler = ExecutionResult (*)(GuestCallContext& call, GuestAddress address,
                                                  std::span<const std::uint64_t> arguments,
                                                  void* context) noexcept;

} // namespace x360port

#endif
