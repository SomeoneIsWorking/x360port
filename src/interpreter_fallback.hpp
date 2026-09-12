#ifndef X360PORT_INTERPRETER_FALLBACK_HPP
#define X360PORT_INTERPRETER_FALLBACK_HPP

#include "x360port/runtime.hpp"

#include <span>

namespace xe::cpu
{
class Processor;
class ThreadState;
} // namespace xe::cpu

namespace x360port
{

[[nodiscard]] ExecutionResult
ExecuteInterpreterFallback(xe::cpu::Processor& processor, xe::cpu::ThreadState& thread_state,
                           GuestAddress address, std::span<const std::uint64_t> arguments,
                           ExecutionLimits limits, JitStatistics& statistics);

} // namespace x360port

#endif
