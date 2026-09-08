#ifndef X360PORT_EXECUTABLE_INVALIDATION_HPP
#define X360PORT_EXECUTABLE_INVALIDATION_HPP

#include "x360port/runtime.hpp"

#include <atomic>
#include <cstdint>

namespace xe
{
class Memory;
}

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

    ~ExecutableInvalidation();

    [[nodiscard]] RuntimeFailure Arm(xe::Memory& memory, CodeRange code_range);
    [[nodiscard]] RuntimeFailure DrainObservedWrites();

    [[nodiscard]] RuntimeFailure Notify(CodeRange code_range, GuestAddress address,
                                        std::uint32_t size);

  private:
    static void OnVirtualWrite(void* context, std::uint32_t virtual_address, std::uint32_t length);
    void RecordVirtualWrite(std::uint32_t virtual_address, std::uint32_t length) noexcept;
    [[nodiscard]] bool Rearm() noexcept;
    void Disarm() noexcept;

    static constexpr std::uint64_t kNoPendingWrite = (static_cast<std::uint64_t>(UINT32_MAX) << 32);

    xe::cpu::Processor& processor_;
    JitStatistics& statistics_;
    xe::Memory* memory_ = nullptr;
    void* callback_handle_ = nullptr;
    CodeRange code_range_{};
    std::atomic<std::uint64_t> pending_write_{kNoPendingWrite};
};

} // namespace x360port

#endif
