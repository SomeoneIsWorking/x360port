#ifndef X360PORT_GUEST_CALL_FRAME_HPP
#define X360PORT_GUEST_CALL_FRAME_HPP

#include <cstdint>

#include "xenia/cpu/ppc/ppc_context.h"

namespace x360port
{

class GuestCallFrame final
{
  public:
    GuestCallFrame(xe::cpu::ppc::PPCContext& context, std::uint64_t return_address) noexcept
        : context_(context), previous_stack_pointer_(context.r[1]),
          previous_link_register_(context.lr)
    {
        context_.r[1] -= 64 + 112;
        context_.lr = return_address;
    }

    GuestCallFrame(const GuestCallFrame&) = delete;
    GuestCallFrame& operator=(const GuestCallFrame&) = delete;

    ~GuestCallFrame()
    {
        context_.lr = previous_link_register_;
        context_.r[1] = previous_stack_pointer_;
    }

  private:
    xe::cpu::ppc::PPCContext& context_;
    std::uint64_t previous_stack_pointer_;
    std::uint64_t previous_link_register_;
};

} // namespace x360port

#endif
