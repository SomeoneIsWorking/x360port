#include "guarded_main.hpp"
#include "x360port/desktop_input.hpp"

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
        std::cerr << "desktop_input_tests: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void KeysAndButtonsFollowTheirEvents()
{
    DesktopInputState state;
    state.SetKey(static_cast<std::uint8_t>(DesktopKey::W), true);
    state.SetKey(static_cast<std::uint8_t>(DesktopKey::RightAlt), true);
    state.SetMouseButton(DesktopMouseButton::Right, true);
    DesktopSnapshot held = state.TakeSnapshot();
    Require(held.IsKeyDown(DesktopKey::W), "a pressed letter key is not down");
    Require(held.IsKeyDown(DesktopKey::RightAlt), "a key in the top word is not down");
    Require(!held.IsKeyDown(DesktopKey::S), "an unpressed key is down");
    Require(held.IsButtonDown(DesktopMouseButton::Right), "a pressed button is not down");
    Require(!held.IsButtonDown(DesktopMouseButton::Left), "an unpressed button is down");

    state.SetKey(static_cast<std::uint8_t>(DesktopKey::W), false);
    state.SetMouseButton(DesktopMouseButton::Right, false);
    DesktopSnapshot released = state.TakeSnapshot();
    Require(!released.IsKeyDown(DesktopKey::W), "a released key is still down");
    Require(released.IsKeyDown(DesktopKey::RightAlt), "releasing one key released another");
    Require(!released.IsButtonDown(DesktopMouseButton::Right), "a released button is still down");
}

void OnlyACapturedPointerTravelsAndEachSnapshotTakesIt()
{
    DesktopInputState state;
    state.AddPointerTravel(40, -7);
    DesktopSnapshot uncaptured = state.TakeSnapshot();
    Require(uncaptured.pointer_dx == 0 && uncaptured.pointer_dy == 0,
            "an uncaptured pointer reported travel");
    Require(!uncaptured.pointer_captured, "the pointer reads captured before capture");

    state.SetPointerCaptured(true);
    state.AddPointerTravel(40, -7);
    state.AddPointerTravel(-10, 3);
    DesktopSnapshot first = state.TakeSnapshot();
    Require(first.pointer_captured, "the captured pointer does not read captured");
    Require(first.pointer_dx == 30 && first.pointer_dy == -4, "travel did not accumulate");
    DesktopSnapshot second = state.TakeSnapshot();
    Require(second.pointer_dx == 0 && second.pointer_dy == 0,
            "a snapshot did not consume the travel it reported");
}

void ReleaseAllClearsEverythingAndReleasesThePointer()
{
    DesktopInputState state;
    state.SetKey(static_cast<std::uint8_t>(DesktopKey::Space), true);
    state.SetMouseButton(DesktopMouseButton::Left, true);
    state.SetPointerCaptured(true);
    state.AddPointerTravel(5, 5);
    state.ReleaseAll();
    DesktopSnapshot cleared = state.TakeSnapshot();
    Require(!cleared.IsKeyDown(DesktopKey::Space), "ReleaseAll left a key down");
    Require(!cleared.IsButtonDown(DesktopMouseButton::Left), "ReleaseAll left a button down");
    Require(!cleared.pointer_captured, "ReleaseAll left the pointer captured");
    Require(cleared.pointer_dx == 0 && cleared.pointer_dy == 0,
            "ReleaseAll kept travel from before the release");
}

[[nodiscard]] int RunTests()
{
    KeysAndButtonsFollowTheirEvents();
    OnlyACapturedPointerTravelsAndEachSnapshotTakesIt();
    ReleaseAllClearsEverythingAndReleasesThePointer();
    std::cout << "desktop_input_tests: 3 cases passed\n";
    return EXIT_SUCCESS;
}

} // namespace

int main() { return x360port::tests::GuardedMain("desktop_input_tests", RunTests); }
