#ifndef X360PORT_DEVICE_DISPATCH_HPP
#define X360PORT_DEVICE_DISPATCH_HPP

#include "x360port/runtime.hpp"

#include <list>

namespace xe
{
class Memory;
}

namespace x360port
{

class DeviceDispatch final
{
  public:
    explicit DeviceDispatch(JitStatistics& statistics) noexcept : statistics_(statistics) {}

    [[nodiscard]] RuntimeFailure Register(xe::Memory& memory, std::uint32_t address,
                                          std::uint32_t mask, std::uint32_t size,
                                          DeviceReadCallback read_callback,
                                          DeviceWriteCallback write_callback, void* context);

  private:
    struct Range
    {
        DeviceReadCallback read_callback = nullptr;
        DeviceWriteCallback write_callback = nullptr;
        void* context = nullptr;
        JitStatistics* statistics = nullptr;
    };

    static std::uint32_t Read(void* ppc_context, void* context, std::uint32_t address) noexcept;
    static void Write(void* ppc_context, void* context, std::uint32_t address,
                      std::uint32_t value) noexcept;

    std::list<Range> ranges_;
    JitStatistics& statistics_;
};

} // namespace x360port

#endif
