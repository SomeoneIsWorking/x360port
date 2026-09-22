#ifndef X360PORT_PAD_MERGE_HPP
#define X360PORT_PAD_MERGE_HPP

#include "x360port/xam_input.hpp"

#include <cstdint>
#include <mutex>

namespace x360port
{

// Combines the title's controller with a host gamepad into the one pad the
// console reports, so a player can use either device at any moment, as on a
// PC. Buttons combine, each trigger takes the further-pressed device, and each
// stick takes the device deflecting it further, keeping its direction whole.
// The packet number advances exactly when the combined state changes.
class PadMerger final
{
  public:
    [[nodiscard]] XamPadSnapshot Merge(const XamPadSnapshot& title,
                                       const XamPadSnapshot& host) noexcept;

  private:
    std::mutex mutex_;
    bool published_ = false;
    XamGamepad last_{};
    std::uint32_t packet_number_ = 0;
};

} // namespace x360port

#endif
