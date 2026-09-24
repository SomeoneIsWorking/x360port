#include "x360port/desktop_input.hpp"

namespace x360port
{
namespace
{

constexpr unsigned kBitsPerWord = 64;

[[nodiscard]] std::uint8_t ButtonBit(DesktopMouseButton button) noexcept
{
    return static_cast<std::uint8_t>(1U << static_cast<unsigned>(button));
}

} // namespace

DesktopSnapshot DesktopInputState::TakeSnapshot() noexcept
{
    // Every bit of the key words has a key in the snapshot.
    static_assert(kKeyWords * kBitsPerWord == decltype(DesktopSnapshot::keys){}.size());
    DesktopSnapshot snapshot;
    for (std::size_t word = 0; word < kKeyWords; ++word)
    {
        std::uint64_t bits = keys_[word].load(std::memory_order_relaxed);
        for (unsigned bit = 0; bit < kBitsPerWord; ++bit)
        {
            if ((bits >> bit & 1U) != 0U)
            {
                snapshot.keys[word * kBitsPerWord + bit] = true;
            }
        }
    }
    snapshot.mouse_buttons = mouse_buttons_.load(std::memory_order_relaxed);
    snapshot.pointer_dx = pointer_dx_.exchange(0, std::memory_order_relaxed);
    snapshot.pointer_dy = pointer_dy_.exchange(0, std::memory_order_relaxed);
    snapshot.pointer_captured = pointer_captured_.load(std::memory_order_relaxed);
    return snapshot;
}

void DesktopInputState::SetKey(std::uint8_t virtual_key, bool down) noexcept
{
    std::uint64_t mask = std::uint64_t{1} << (virtual_key % kBitsPerWord);
    std::atomic<std::uint64_t>& word = keys_[virtual_key / kBitsPerWord];
    if (down)
    {
        word.fetch_or(mask, std::memory_order_relaxed);
    }
    else
    {
        word.fetch_and(~mask, std::memory_order_relaxed);
    }
}

void DesktopInputState::SetMouseButton(DesktopMouseButton button, bool down) noexcept
{
    if (down)
    {
        mouse_buttons_.fetch_or(ButtonBit(button), std::memory_order_relaxed);
    }
    else
    {
        mouse_buttons_.fetch_and(static_cast<std::uint8_t>(~ButtonBit(button)),
                                 std::memory_order_relaxed);
    }
}

void DesktopInputState::AddPointerTravel(std::int32_t dx, std::int32_t dy) noexcept
{
    if (!pointer_captured_.load(std::memory_order_relaxed))
    {
        return;
    }
    pointer_dx_.fetch_add(dx, std::memory_order_relaxed);
    pointer_dy_.fetch_add(dy, std::memory_order_relaxed);
}

void DesktopInputState::SetPointerCaptured(bool captured) noexcept
{
    pointer_captured_.store(captured, std::memory_order_relaxed);
    if (!captured)
    {
        pointer_dx_.store(0, std::memory_order_relaxed);
        pointer_dy_.store(0, std::memory_order_relaxed);
    }
}

void DesktopInputState::ReleaseAll() noexcept
{
    for (std::atomic<std::uint64_t>& word : keys_)
    {
        word.store(0, std::memory_order_relaxed);
    }
    mouse_buttons_.store(0, std::memory_order_relaxed);
    SetPointerCaptured(false);
}

} // namespace x360port
