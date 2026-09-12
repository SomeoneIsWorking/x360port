#ifndef X360PORT_GUEST_EXECUTION_BUDGET_HPP
#define X360PORT_GUEST_EXECUTION_BUDGET_HPP

#include "x360port/validation.hpp"

#include <exception>
#include <string_view>

#include "xenia/cpu/ppc/ppc_context.h"

namespace x360port
{

inline thread_local xe::cpu::ppc::GuestExecutionBudget* active_guest_execution_budget = nullptr;

struct GuestImportRefusal
{
    std::string_view library;
    std::uint32_t ordinal = 0;
    ImportRefusalReason reason = ImportRefusalReason::UnsupportedService;
};

inline thread_local GuestImportRefusal* active_guest_import_refusal = nullptr;

inline void MarkGuestImportRefused(std::string_view library, std::uint32_t ordinal,
                                   ImportRefusalReason reason) noexcept
{
    if (active_guest_execution_budget == nullptr || active_guest_import_refusal == nullptr)
    {
        std::terminate();
    }
    if (active_guest_execution_budget->exit_reason == xe::cpu::ppc::GuestExecutionExitReason::kNone)
    {
        *active_guest_import_refusal = {library, ordinal, reason};
        active_guest_execution_budget->exit_reason =
            xe::cpu::ppc::GuestExecutionExitReason::kHostServiceRefused;
    }
}

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
                              xe::cpu::ppc::GuestExecutionBudget& budget,
                              GuestImportRefusal& refusal) noexcept
        : context_(context), previous_(context.execution_budget),
          previous_refusal_(active_guest_import_refusal)
    {
        context_.execution_budget = &budget;
        previous_active_ = active_guest_execution_budget;
        active_guest_execution_budget = &budget;
        active_guest_import_refusal = &refusal;
    }

    GuestExecutionBudgetScope(const GuestExecutionBudgetScope&) = delete;
    GuestExecutionBudgetScope& operator=(const GuestExecutionBudgetScope&) = delete;

    ~GuestExecutionBudgetScope()
    {
        active_guest_import_refusal = previous_refusal_;
        active_guest_execution_budget = previous_active_;
        context_.execution_budget = previous_;
    }

  private:
    xe::cpu::ppc::PPCContext& context_;
    xe::cpu::ppc::GuestExecutionBudget* previous_;
    xe::cpu::ppc::GuestExecutionBudget* previous_active_ = nullptr;
    GuestImportRefusal* previous_refusal_ = nullptr;
};

} // namespace x360port

#endif
