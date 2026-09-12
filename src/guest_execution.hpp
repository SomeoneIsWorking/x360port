#ifndef X360PORT_GUEST_EXECUTION_HPP
#define X360PORT_GUEST_EXECUTION_HPP

#include "x360port/runtime.hpp"

#include <span>

namespace xe::cpu
{
class Function;
class ThreadState;
} // namespace xe::cpu

namespace x360port
{

[[nodiscard]] RuntimeFailure PrepareGuestArguments(xe::cpu::ThreadState& thread_state,
                                                   std::span<const std::uint64_t> arguments);

[[nodiscard]] ExecutionResult ExecuteGuestFunction(xe::cpu::Function& function,
                                                   xe::cpu::ThreadState& thread_state,
                                                   std::span<const std::uint64_t> arguments,
                                                   ExecutionLimits limits);

} // namespace x360port

#endif
