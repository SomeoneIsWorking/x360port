#include "executable_invalidation.hpp"

#include <unordered_set>

#include "xenia/cpu/function.h"
#include "xenia/cpu/processor.h"

namespace x360port
{
namespace
{

[[nodiscard]] RuntimeFailure Failure(RuntimeError error, const char* detail)
{
    return RuntimeFailure{error, detail};
}

} // namespace

RuntimeFailure ExecutableInvalidation::Notify(CodeRange code_range, GuestAddress address,
                                              std::uint32_t size)
{
    const std::uint64_t code_end = static_cast<std::uint64_t>(code_range.base) + code_range.size;
    const std::uint64_t write_end = static_cast<std::uint64_t>(address) + size;
    if (size == 0 || address < code_range.base || write_end > code_end)
    {
        return Failure(
            RuntimeError::ExecutableRangeInvalid,
            "executable write must be non-empty and inside the authenticated code range");
    }

    std::unordered_set<GuestAddress> invalidated;
    const GuestAddress first_instruction = address & ~GuestAddress{3};
    for (std::uint64_t instruction = first_instruction; instruction < write_end; instruction += 4)
    {
        for (xe::cpu::Function* function :
             processor_.FindFunctionsWithAddress(static_cast<GuestAddress>(instruction)))
        {
            if (invalidated.insert(function->address()).second)
            {
                processor_.RemoveFunctionByAddress(function->address());
                ++statistics_.translation_invalidations;
            }
        }
    }
    return {};
}

} // namespace x360port
