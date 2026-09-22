#include "pad_merge.hpp"

#include <algorithm>

namespace x360port
{
namespace
{

[[nodiscard]] std::int64_t Magnitude(std::int16_t x, std::int16_t y) noexcept
{
    return std::int64_t{x} * x + std::int64_t{y} * y;
}

// Each stick is taken whole from the device deflecting it further, so a
// diagonal from one device is never recombined with an axis of the other.
void TakeFurtherStick(std::int16_t& x, std::int16_t& y, std::int16_t other_x,
                      std::int16_t other_y) noexcept
{
    if (Magnitude(other_x, other_y) > Magnitude(x, y))
    {
        x = other_x;
        y = other_y;
    }
}

[[nodiscard]] XamGamepad Combine(const XamGamepad& title, const XamGamepad& host) noexcept
{
    XamGamepad combined = title;
    combined.buttons = static_cast<std::uint16_t>(title.buttons | host.buttons);
    combined.left_trigger = std::max(title.left_trigger, host.left_trigger);
    combined.right_trigger = std::max(title.right_trigger, host.right_trigger);
    TakeFurtherStick(combined.thumb_lx, combined.thumb_ly, host.thumb_lx, host.thumb_ly);
    TakeFurtherStick(combined.thumb_rx, combined.thumb_ry, host.thumb_rx, host.thumb_ry);
    return combined;
}

[[nodiscard]] bool SameGamepad(const XamGamepad& left, const XamGamepad& right) noexcept
{
    return left.buttons == right.buttons && left.left_trigger == right.left_trigger &&
           left.right_trigger == right.right_trigger && left.thumb_lx == right.thumb_lx &&
           left.thumb_ly == right.thumb_ly && left.thumb_rx == right.thumb_rx &&
           left.thumb_ry == right.thumb_ry;
}

} // namespace

XamPadSnapshot PadMerger::Merge(const XamPadSnapshot& title, const XamPadSnapshot& host) noexcept
{
    if (!title.connected && !host.connected)
    {
        return {};
    }
    XamGamepad combined{};
    if (title.connected && host.connected)
    {
        combined = Combine(title.gamepad, host.gamepad);
    }
    else
    {
        combined = title.connected ? title.gamepad : host.gamepad;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (!published_ || !SameGamepad(combined, last_))
    {
        published_ = true;
        last_ = combined;
        ++packet_number_;
    }
    return {.connected = true, .packet_number = packet_number_, .gamepad = combined};
}

} // namespace x360port
