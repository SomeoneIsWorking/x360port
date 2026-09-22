#include "pad_merge.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace
{

using namespace x360port;

void Require(bool condition, std::string_view message)
{
    if (!condition)
    {
        std::cerr << "pad_merge_tests: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

XamPadSnapshot Pad(XamGamepad gamepad)
{
    return {.connected = true, .packet_number = 0, .gamepad = gamepad};
}

void NeitherDeviceIsNoPad()
{
    PadMerger merger;
    Require(!merger.Merge({}, {}).connected, "two absent devices reported a pad");
}

void OneDeviceAnswersAlone()
{
    PadMerger merger;
    XamGamepad host{.buttons = 0x1000, .thumb_lx = 900};
    XamPadSnapshot merged = merger.Merge({}, Pad(host));
    Require(merged.connected, "a lone host gamepad was not reported");
    Require(merged.gamepad.buttons == 0x1000 && merged.gamepad.thumb_lx == 900,
            "a lone host gamepad was altered");
}

void BothDevicesCombine()
{
    PadMerger merger;
    XamGamepad title{.buttons = 0x0010,
                     .left_trigger = 200,
                     .right_trigger = 10,
                     .thumb_lx = 0,
                     .thumb_ly = 32767,
                     .thumb_rx = 3000,
                     .thumb_ry = 3000};
    XamGamepad host{.buttons = 0x1000,
                    .left_trigger = 50,
                    .right_trigger = 255,
                    .thumb_lx = 100,
                    .thumb_ly = 100,
                    .thumb_rx = -20000,
                    .thumb_ry = 0};
    XamGamepad merged = merger.Merge(Pad(title), Pad(host)).gamepad;
    Require(merged.buttons == 0x1010, "buttons did not combine");
    Require(merged.left_trigger == 200 && merged.right_trigger == 255,
            "each trigger did not take the further-pressed device");
    Require(merged.thumb_lx == 0 && merged.thumb_ly == 32767,
            "the left stick did not come whole from the device deflecting it further");
    Require(merged.thumb_rx == -20000 && merged.thumb_ry == 0,
            "the right stick did not come whole from the device deflecting it further");
}

void PacketAdvancesExactlyOnChange()
{
    PadMerger merger;
    XamGamepad idle{};
    XamGamepad pressed{.buttons = 0x2000};
    std::uint32_t first = merger.Merge(Pad(idle), {}).packet_number;
    Require(merger.Merge(Pad(idle), {}).packet_number == first,
            "an unchanged pad advanced the packet number");
    // The same combined state from the other device is still no change.
    Require(merger.Merge({}, Pad(idle)).packet_number == first,
            "switching devices without a change advanced the packet number");
    Require(merger.Merge(Pad(idle), Pad(pressed)).packet_number == first + 1,
            "a changed pad did not advance the packet number");
}

} // namespace

int main()
{
    NeitherDeviceIsNoPad();
    OneDeviceAnswersAlone();
    BothDevicesCombine();
    PacketAdvancesExactlyOnChange();
    std::cout << "pad_merge_tests: 4 cases passed\n";
    return EXIT_SUCCESS;
}
