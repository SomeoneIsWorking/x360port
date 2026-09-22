#ifndef X360PORT_GUEST_RANGE_RESERVATION_HPP
#define X360PORT_GUEST_RANGE_RESERVATION_HPP

#include <cstdint>

#include "xenia/memory.h"

namespace x360port
{

// Releases a reserved guest heap range unless the caller commits it, so a
// failure part-way through a multi-step allocation cannot leak the range.
class GuestRangeReservation final
{
  public:
    GuestRangeReservation(xe::BaseHeap& heap, std::uint32_t address) noexcept
        : heap_(&heap), address_(address)
    {
    }

    GuestRangeReservation(const GuestRangeReservation&) = delete;
    GuestRangeReservation& operator=(const GuestRangeReservation&) = delete;
    GuestRangeReservation(GuestRangeReservation&&) = delete;
    GuestRangeReservation& operator=(GuestRangeReservation&&) = delete;

    ~GuestRangeReservation()
    {
        if (heap_ != nullptr)
        {
            static_cast<void>(heap_->Release(address_));
        }
    }

    void Commit() noexcept { heap_ = nullptr; }

  private:
    xe::BaseHeap* heap_;
    std::uint32_t address_;
};

} // namespace x360port

#endif
