#include "guest_execution.hpp"

#include "guest_call_frame.hpp"
#include "guest_execution_budget.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include "xenia/cpu/function.h"
#include "xenia/cpu/ppc/ppc_context.h"
#include "xenia/cpu/thread_state.h"

namespace x360port
{
namespace
{

constexpr std::uint32_t kReturnAddress = 0xBCBCBCBC;
constexpr std::size_t kRegisterArgumentCount = 8;

[[nodiscard]] std::string_view RefusalReasonText(ImportRefusalReason reason) noexcept
{
    switch (reason)
    {
    case ImportRefusalReason::UnsupportedService:
        return "unsupported service";
    case ImportRefusalReason::InvalidGuestMemory:
        return "invalid guest memory";
    case ImportRefusalReason::InvalidArguments:
        return "invalid arguments";
    case ImportRefusalReason::HostUnavailable:
        return "host service unavailable";
    }
    return "unknown refusal";
}

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
    GuestImportRefusal import_refusal;
    RuntimeFailure override_failure;
    const GuestExecutionBudgetScope budget_scope(*context, execution_budget, import_refusal,
                                                 override_failure);
    const GuestCallFrame call_frame(*context, kReturnAddress);
    const bool executed = function.Call(&thread_state, kReturnAddress);
    if (execution_budget.exit_reason != xe::cpu::ppc::GuestExecutionExitReason::kNone)
    {
        if (execution_budget.exit_reason ==
            xe::cpu::ppc::GuestExecutionExitReason::kExecutableWriteObserved)
        {
            return {
                RuntimeFailure{RuntimeError::ExecutionInvalidated,
                               "Xenia stopped the guest call after observing an executable write"},
                0};
        }
        if (execution_budget.exit_reason ==
            xe::cpu::ppc::GuestExecutionExitReason::kHostServiceRefused)
        {
            return {RuntimeFailure{
                        RuntimeError::ImportServiceRefused,
                        "host import " + std::string(import_refusal.library) + " ordinal " +
                            std::to_string(import_refusal.ordinal) +
                            " refused: " + std::string(RefusalReasonText(import_refusal.reason))},
                    0};
        }
        if (execution_budget.exit_reason ==
            xe::cpu::ppc::GuestExecutionExitReason::kNativeOverrideFailed)
        {
            return {std::move(override_failure), 0};
        }
        if (execution_budget.exit_reason ==
            xe::cpu::ppc::GuestExecutionExitReason::kBlockBudgetExceeded)
        {
            return {
                RuntimeFailure{
                    RuntimeError::ExecutionBudgetExceeded,
                    "Xenia stopped the guest call after exhausting its translated-block budget"},
                0};
        }
        return {RuntimeFailure{RuntimeError::ExecutionFailed,
                               "Xenia stopped the guest call with an unknown exit reason"},
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
