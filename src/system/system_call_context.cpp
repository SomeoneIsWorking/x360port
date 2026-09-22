#include "system_call_context.hpp"

#include "../guest_call_frame.hpp"
#include "../guest_execution.hpp"

#include "xenia/cpu/function.h"
#include "xenia/cpu/processor.h"
#include "xenia/cpu/thread_state.h"

namespace x360port
{
namespace
{

// Any address the original body cannot reach by returning normally; Xenia
// ends the call when the body branches to it through lr.
constexpr std::uint32_t kReturnAddress = 0xBCBCBCBC;

} // namespace

SystemCallContext::SystemCallContext(xe::Memory& memory, xe::cpu::Processor& processor) noexcept
    : processor_(&processor)
{
    memory_.Initialize(memory);
}

RuntimeFailure SystemCallContext::ReadMappedGuestMemory(GuestAddress address,
                                                        std::span<std::byte> bytes) const
{
    return memory_.ReadMapped(address, bytes);
}

RuntimeFailure SystemCallContext::WriteMappedGuestMemory(GuestAddress address,
                                                         std::span<const std::byte> bytes)
{
    return memory_.WriteMapped(address, bytes);
}

ExecutionResult SystemCallContext::CallOriginalBody(GuestAddress address,
                                                    std::span<const std::uint64_t> arguments)
{
    xe::cpu::ThreadState* thread_state = xe::cpu::ThreadState::Get();
    if (thread_state == nullptr)
    {
        return {RuntimeFailure{RuntimeError::ExecutionFailed,
                               "an original call must be made from the guest thread that "
                               "entered the override"},
                0};
    }
    auto* function = dynamic_cast<xe::cpu::GuestFunction*>(processor_->QueryFunction(address));
    if (function == nullptr || function->machine_code() == nullptr)
    {
        return {RuntimeFailure{RuntimeError::OverrideNotInstalled,
                               "the overridden guest body has no translated code to re-enter"},
                0};
    }
    if (const RuntimeFailure failure = PrepareGuestArguments(*thread_state, arguments))
    {
        return {failure, 0};
    }
    auto& context = *thread_state->context();
    const GuestCallFrame call_frame(context, kReturnAddress);
    if (!function->Call(thread_state, kReturnAddress))
    {
        return {RuntimeFailure{RuntimeError::ExecutionFailed,
                               "Xenia's translated original body refused execution"},
                0};
    }
    return {{}, context.r[3]};
}

} // namespace x360port
