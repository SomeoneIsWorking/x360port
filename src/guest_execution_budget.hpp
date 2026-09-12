#ifndef X360PORT_GUEST_EXECUTION_BUDGET_HPP
#define X360PORT_GUEST_EXECUTION_BUDGET_HPP

#include "xenia/cpu/ppc/ppc_context.h"

namespace x360port
{

class GuestExecutionBudgetScope final
{
  public:
    GuestExecutionBudgetScope(xe::cpu::ppc::PPCContext& context,
                              xe::cpu::ppc::GuestExecutionBudget& budget) noexcept
        : context_(context), previous_(context.execution_budget)
    {
        context_.execution_budget = &budget;
    }

    GuestExecutionBudgetScope(const GuestExecutionBudgetScope&) = delete;
    GuestExecutionBudgetScope& operator=(const GuestExecutionBudgetScope&) = delete;

    ~GuestExecutionBudgetScope() { context_.execution_budget = previous_; }

  private:
    xe::cpu::ppc::PPCContext& context_;
    xe::cpu::ppc::GuestExecutionBudget* previous_;
};

} // namespace x360port

#endif
