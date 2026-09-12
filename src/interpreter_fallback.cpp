#include "interpreter_fallback.hpp"

#include "guest_execution.hpp"

#include <string>
#include <utility>

#include "xenia/cpu/function.h"
#include "xenia/cpu/ppc/ppc_interpreter.h"
#include "xenia/cpu/processor.h"
#include "xenia/cpu/thread_state.h"

namespace x360port
{
namespace
{

[[nodiscard]] RuntimeFailure Failure(RuntimeError error, std::string detail)
{
    return RuntimeFailure{error, std::move(detail)};
}

} // namespace

ExecutionResult ExecuteInterpreterFallback(xe::cpu::Processor& processor,
                                           xe::cpu::ThreadState& thread_state, GuestAddress address,
                                           std::span<const std::uint64_t> arguments,
                                           ExecutionLimits limits, JitStatistics& statistics)
{
    if (const RuntimeFailure failure = PrepareGuestArguments(thread_state, arguments))
    {
        return {failure, 0};
    }
    if (dynamic_cast<xe::cpu::GuestFunction*>(processor.LookupFunction(address)) == nullptr)
    {
        return {Failure(RuntimeError::TranslationFailed,
                        "Xenia could not resolve the requested guest function for translation or "
                        "fallback"),
                0};
    }

    ++statistics.interpreter_fallback_entries;
    const auto fallback =
        processor.ExecuteInterpreter(&thread_state, address, limits.max_interpreter_instructions);
    statistics.interpreter_fallback_instructions += fallback.instructions_executed;
    switch (fallback.exit_reason)
    {
    case xe::cpu::ppc::PPCInterpreterExitReason::kCompleted:
        ++statistics.execution_calls;
        return {{}, thread_state.context()->r[3]};
    case xe::cpu::ppc::PPCInterpreterExitReason::kUnsupportedInstruction:
        ++statistics.interpreter_fallback_unsupported;
        return {Failure(RuntimeError::InterpreterFallbackUnsupported,
                        "bounded interpreter refused guest PC " +
                            std::to_string(fallback.guest_pc) + " opcode " +
                            std::to_string(fallback.instruction)),
                0};
    case xe::cpu::ppc::PPCInterpreterExitReason::kMemoryFault:
        ++statistics.interpreter_fallback_memory_failures;
        return {Failure(RuntimeError::InterpreterFallbackMemoryInvalid,
                        "bounded interpreter could not access guest PC " +
                            std::to_string(fallback.guest_pc)),
                0};
    case xe::cpu::ppc::PPCInterpreterExitReason::kInstructionBudgetExceeded:
        ++statistics.interpreter_fallback_budget_exhaustions;
        return {Failure(RuntimeError::InterpreterFallbackBudgetExceeded,
                        "bounded interpreter exhausted its instruction budget at guest PC " +
                            std::to_string(fallback.guest_pc)),
                0};
    case xe::cpu::ppc::PPCInterpreterExitReason::kInvalidEntry:
        return {Failure(RuntimeError::TranslationFailed,
                        "Xenia could not translate or interpret the requested guest function"),
                0};
    }
}

} // namespace x360port
