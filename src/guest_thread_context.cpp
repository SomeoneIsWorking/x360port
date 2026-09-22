#include "guest_thread_context.hpp"

#include "xenia/cpu/processor.h"
#include "xenia/cpu/thread_state.h"
#include "xenia/memory.h"

namespace x360port
{

GuestThreadContext::~GuestThreadContext() { Reset(); }

RuntimeFailure GuestThreadContext::Initialize(xe::Memory& memory, xe::cpu::Processor& processor)
{
    memory_ = &memory;
    stack_address_ = memory.SystemHeapAlloc(kStackSize);
    if (stack_address_ == 0)
    {
        return RuntimeFailure{RuntimeError::StackAllocationFailed,
                              "Xenia could not allocate the bounded guest call stack"};
    }
    pcr_address_ = memory.SystemHeapAlloc(kPcrSize);
    if (pcr_address_ == 0)
    {
        return RuntimeFailure{RuntimeError::StackAllocationFailed,
                              "Xenia could not allocate the guest thread PCR"};
    }
    state_ = std::make_unique<xe::cpu::ThreadState>(&processor, kThreadId,
                                                    stack_address_ + kStackSize, pcr_address_);
    return {};
}

void GuestThreadContext::Reset() noexcept
{
    state_.reset();
    if (memory_ != nullptr)
    {
        if (stack_address_ != 0)
        {
            memory_->SystemHeapFree(stack_address_);
        }
        if (pcr_address_ != 0)
        {
            memory_->SystemHeapFree(pcr_address_);
        }
    }
    stack_address_ = 0;
    pcr_address_ = 0;
    memory_ = nullptr;
}

} // namespace x360port
