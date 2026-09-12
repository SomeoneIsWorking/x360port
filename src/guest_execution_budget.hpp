#ifndef X360PORT_GUEST_EXECUTION_BUDGET_HPP
#define X360PORT_GUEST_EXECUTION_BUDGET_HPP

#include "xenia/cpu/ppc/ppc_context.h"

namespace x360port
{

inline thread_local xe::cpu::ppc::GuestExecutionBudget* active_guest_execution_budget = nullptr;

inline void MarkGuestExecutableWriteObserved() noexcept
{
    if (active_guest_execution_budget != nullptr &&
        active_guest_execution_budget->exit_reason == xe::cpu::ppc::GuestExecutionExitReason::kNone)
    {
        active_guest_execution_budget->exit_reason =
            xe::cpu::ppc::GuestExecutionExitReason::kExecutableWriteObserved;
    }
}

class GuestExecutionBudgetScope final
{
  public:
    GuestExecutionBudgetScope(xe::cpu::ppc::PPCContext& context,
                              xe::cpu::ppc::GuestExecutionBudget& budget) noexcept
        : context_(context), previous_(context.execution_budget)
    {
        context_.execution_budget = &budget;
        previous_active_ = active_guest_execution_budget;
        active_guest_execution_budget = &budget;
    }

    GuestExecutionBudgetScope(const GuestExecutionBudgetScope&) = delete;
    GuestExecutionBudgetScope& operator=(const GuestExecutionBudgetScope&) = delete;

    ~GuestExecutionBudgetScope()
    {
        active_guest_execution_budget = previous_active_;
        context_.execution_budget = previous_;
    }

  private:
    xe::cpu::ppc::PPCContext& context_;
    xe::cpu::ppc::GuestExecutionBudget* previous_;
    xe::cpu::ppc::GuestExecutionBudget* previous_active_ = nullptr;
};

} // namespace x360port

#endif
