#ifndef X360PORT_EXECUTABLE_INVALIDATION_HPP
#define X360PORT_EXECUTABLE_INVALIDATION_HPP

#include "x360port/runtime.hpp"

namespace xe::cpu
{
class Processor;
}

namespace x360port
{

class ExecutableInvalidation final
{
  public:
    ExecutableInvalidation(xe::cpu::Processor& processor, JitStatistics& statistics) noexcept
        : processor_(processor), statistics_(statistics)
    {
    }

    [[nodiscard]] RuntimeFailure Notify(CodeRange code_range, GuestAddress address,
                                        std::uint32_t size);

  private:
    xe::cpu::Processor& processor_;
    JitStatistics& statistics_;
};

} // namespace x360port

#endif
