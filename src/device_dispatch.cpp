#include "device_dispatch.hpp"

#include "xenia/memory.h"

namespace x360port
{
namespace
{

[[nodiscard]] RuntimeFailure Failure(RuntimeError error, const char* detail)
{
    return RuntimeFailure{error, detail};
}

} // namespace

RuntimeFailure DeviceDispatch::Register(xe::Memory& memory, std::uint32_t address,
                                        std::uint32_t mask, std::uint32_t size,
                                        DeviceReadCallback read_callback,
                                        DeviceWriteCallback write_callback, void* context)
{
    if (size == 0 || read_callback == nullptr || write_callback == nullptr ||
        (address & ~mask) >= size)
    {
        return Failure(RuntimeError::DeviceRangeInvalid,
                       "device range requires a non-empty aligned masked range and both callbacks");
    }

    ranges_.push_back(Range{read_callback, write_callback, context, &statistics_});
    Range& range = ranges_.back();
    if (!memory.AddVirtualMappedRange(address, mask, size, &range, Read, Write))
    {
        ranges_.pop_back();
        return Failure(RuntimeError::DeviceRangeRegistrationFailed,
                       "Xenia refused the virtual device-memory range");
    }
    return {};
}

std::uint32_t DeviceDispatch::Read(void*, void* context, std::uint32_t address) noexcept
{
    auto& range = *static_cast<Range*>(context);
    ++range.statistics->device_read_calls;
    return range.read_callback(address, range.context);
}

void DeviceDispatch::Write(void*, void* context, std::uint32_t address,
                           std::uint32_t value) noexcept
{
    auto& range = *static_cast<Range*>(context);
    ++range.statistics->device_write_calls;
    range.write_callback(address, value, range.context);
}

} // namespace x360port
