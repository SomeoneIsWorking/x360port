#ifndef X360PORT_DESKTOP_INPUT_HPP
#define X360PORT_DESKTOP_INPUT_HPP

#include <array>
#include <atomic>
#include <bitset>
#include <cstdint>

namespace x360port
{

// A desktop key, numbered by its Windows virtual-key code. Only the keys a
// game binding plausibly names are spelled out; the window reports any other
// code in the same numbering.
enum class DesktopKey : std::uint8_t
{
    Backspace = 0x08,
    Tab = 0x09,
    Return = 0x0D,
    Escape = 0x1B,
    Space = 0x20,
    PageUp = 0x21,
    PageDown = 0x22,
    End = 0x23,
    Home = 0x24,
    Left = 0x25,
    Up = 0x26,
    Right = 0x27,
    Down = 0x28,
    Digit0 = 0x30,
    Digit1,
    Digit2,
    Digit3,
    Digit4,
    Digit5,
    Digit6,
    Digit7,
    Digit8,
    Digit9,
    A = 0x41,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,
    F1 = 0x70,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
    LeftShift = 0xA0,
    RightShift,
    LeftControl,
    RightControl,
    LeftAlt,
    RightAlt,
};

enum class DesktopMouseButton : std::uint8_t
{
    Left,
    Right,
    Middle,
    Back,
    Forward,
};

// One consistent reading of the desktop devices, taken by the title's input
// owner each time the guest polls its controller.
struct DesktopSnapshot
{
    std::bitset<256> keys;
    std::uint8_t mouse_buttons = 0;
    // Pointer travel since the previous snapshot, in window pixels, x to the
    // right and y downwards. Only a captured pointer travels: an uncaptured
    // cursor is the player using the desktop, not the game.
    std::int32_t pointer_dx = 0;
    std::int32_t pointer_dy = 0;
    bool pointer_captured = false;

    [[nodiscard]] bool IsKeyDown(DesktopKey key) const noexcept
    {
        const auto index = static_cast<std::size_t>(key);
        return index < keys.size() && keys[index];
    }
    [[nodiscard]] bool IsButtonDown(DesktopMouseButton button) const noexcept
    {
        return (mouse_buttons & (1U << static_cast<unsigned>(button))) != 0U;
    }
};

// Keyboard and mouse state the game window writes and a title's controller
// source reads. The window writes from its UI thread; any guest thread may
// take a snapshot. The title owns the object and keeps it alive for the whole
// session; the session only writes to it.
class DesktopInputState final
{
  public:
    // Takes the current state and consumes the pointer travel it reports.
    [[nodiscard]] DesktopSnapshot TakeSnapshot() noexcept;

    // Writers, for the window that owns the devices.
    void SetKey(std::uint8_t virtual_key, bool down) noexcept;
    void SetMouseButton(DesktopMouseButton button, bool down) noexcept;
    // Ignored while the pointer is not captured.
    void AddPointerTravel(std::int32_t dx, std::int32_t dy) noexcept;
    void SetPointerCaptured(bool captured) noexcept;
    // Everything up, the pointer released, and pending travel discarded: the
    // window lost focus and no longer sees the key-up events.
    void ReleaseAll() noexcept;

  private:
    static constexpr std::size_t kKeyWords = 4;
    std::array<std::atomic<std::uint64_t>, kKeyWords> keys_{};
    std::atomic<std::uint8_t> mouse_buttons_{0};
    std::atomic<std::int32_t> pointer_dx_{0};
    std::atomic<std::int32_t> pointer_dy_{0};
    std::atomic<bool> pointer_captured_{false};
};

} // namespace x360port

#endif
