#include "guest_fault_guard.hpp"

#include "guest_execution_budget.hpp"

#include <cstddef>

#include "xenia/base/exception_handler.h"
#include "xenia/cpu/backend/code_cache.h"
#include "xenia/cpu/function.h"
#include "xenia/memory.h"

namespace x360port
{

GuestFaultGuard::GuestFaultGuard(xe::cpu::backend::CodeCache& code_cache,
                                 xe::Memory& memory) noexcept
    : code_cache_(code_cache), memory_(memory)
{
    // Handlers run in installation order, so constructing this guard after the
    // memory subsystem leaves MMIO and write-watch handling ahead of it. Those
    // resolve the faults they own and this guard never sees them.
    xe::ExceptionHandler::Install(&GuestFaultGuard::Dispatch, this);
}

GuestFaultGuard::~GuestFaultGuard()
{
    xe::ExceptionHandler::Uninstall(&GuestFaultGuard::Dispatch, this);
}

bool GuestFaultGuard::Dispatch(xe::Exception* exception, void* context)
{
    if (exception == nullptr || context == nullptr)
    {
        return false;
    }
    return static_cast<GuestFaultGuard*>(context)->Claim(*exception);
}

bool GuestFaultGuard::InGeneratedCode(std::uint64_t host_pc) const noexcept
{
    const auto base = static_cast<std::uint64_t>(code_cache_.execute_base_address());
    const auto size = static_cast<std::uint64_t>(code_cache_.total_size());
    return host_pc >= base && host_pc < base + size;
}

bool GuestFaultGuard::Claim(xe::Exception& exception) noexcept
{
    if (exception.code() != xe::Exception::Code::kAccessViolation)
    {
        return false;
    }
    // Only a call this runtime started can be unwound here. A fault on any
    // other thread, or outside a guest call, belongs to the host.
    xe::cpu::ppc::GuestExecutionBudget* budget = active_guest_execution_budget;
    if (budget == nullptr || budget->exit_reason != xe::cpu::ppc::GuestExecutionExitReason::kNone)
    {
        return false;
    }
    // A fault in a native override or elsewhere in the host is a host defect
    // and must keep its original diagnosis rather than becoming a guest fault.
    const std::uint64_t host_pc = exception.pc();
    if (!InGeneratedCode(host_pc))
    {
        return false;
    }
    auto* function = code_cache_.LookupFunction(host_pc);
    if (function == nullptr)
    {
        return false;
    }
    const std::size_t epilog_offset = function->epilog_offset();
    if (epilog_offset == 0 || function->machine_code() == nullptr ||
        epilog_offset >= function->machine_code_length())
    {
        return false;
    }

    const auto fault_address = exception.fault_address();
    const auto virtual_base = reinterpret_cast<std::uint64_t>(memory_.virtual_membase());
    if (fault_address >= virtual_base)
    {
        // Derived from the existing base pointer rather than cast from an
        // integer, and converted by the owner so the mapping stays in one place.
        const std::byte* host_address =
            reinterpret_cast<const std::byte*>(memory_.virtual_membase()) +
            (fault_address - virtual_base);
        budget->fault_guest_address = memory_.HostToGuestVirtual(host_address);
    }
    budget->fault_was_write =
        exception.access_violation_operation() == xe::Exception::AccessViolationOperation::kWrite;
    budget->exit_reason = xe::cpu::ppc::GuestExecutionExitReason::kGuestAccessViolation;

    // The epilogue restores exactly the frame the prolog established, which is
    // what a budget exit relies on when it jumps here from a block boundary.
    exception.set_resume_pc(reinterpret_cast<std::uint64_t>(function->machine_code()) +
                            epilog_offset);
    return true;
}

} // namespace x360port
