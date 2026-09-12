#include "guest_execution.hpp"

#include "guest_call_frame.hpp"
#include "guest_execution_budget.hpp"

#include <cstddef>

#include "xenia/cpu/function.h"
#include "xenia/cpu/ppc/ppc_context.h"
#include "xenia/cpu/thread_state.h"

namespace x360port
{
namespace
{

constexpr std::uint32_t kReturnAddress = 0xBCBCBCBC;
constexpr std::size_t kRegisterArgumentCount = 8;

} // namespace

ExecutionResult ExecuteGuestFunction(xe::cpu::Function& function,
                                     xe::cpu::ThreadState& thread_state,
                                     std::span<const std::uint64_t> arguments,
                                     ExecutionLimits limits)
{
    if (arguments.size() > kRegisterArgumentCount)
    {
        return {
            RuntimeFailure{RuntimeError::ExecutionFailed,
                           "this bounded call contract accepts at most eight register arguments"},
            0};
    }
    if (limits.max_guest_blocks == 0)
    {
        return {RuntimeFailure{RuntimeError::ExecutionBudgetInvalid,
                               "guest execution requires a non-zero translated-block budget"},
                0};
    }

    auto* context = thread_state.context();
    for (std::size_t index = 0; index < arguments.size(); ++index)
    {
        context->r[3 + index] = arguments[index];
    }
    xe::cpu::ppc::GuestExecutionBudget execution_budget{
        limits.max_guest_blocks, xe::cpu::ppc::GuestExecutionExitReason::kNone, 0};
    const GuestExecutionBudgetScope budget_scope(*context, execution_budget);
    const GuestCallFrame call_frame(*context, kReturnAddress);
    const bool executed = function.Call(&thread_state, kReturnAddress);
    if (execution_budget.exit_reason != xe::cpu::ppc::GuestExecutionExitReason::kNone)
    {
        return {RuntimeFailure{
                    RuntimeError::ExecutionBudgetExceeded,
                    "Xenia stopped the guest call after exhausting its translated-block budget"},
                0};
    }
    if (!executed)
    {
        return {RuntimeFailure{RuntimeError::ExecutionFailed,
                               "Xenia's translated guest function refused execution"},
                0};
    }

    return {{}, context->r[3]};
}

} // namespace x360port
