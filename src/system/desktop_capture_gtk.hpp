#ifndef X360PORT_DESKTOP_CAPTURE_GTK_HPP
#define X360PORT_DESKTOP_CAPTURE_GTK_HPP

#include "x360port/desktop_input.hpp"

#include <gtk/gtk.h>

#include <cstdint>

#include "xenia/ui/window.h"
#include "xenia/ui/window_listener.h"

namespace x360port
{

// Feeds a game window's keyboard and mouse into a DesktopInputState, with the
// PC conventions for a game that aims with the mouse: a click in the window
// captures the pointer (hidden, held inside the window, reporting travel),
// and that first click is not a game input; losing focus releases the pointer
// and every held key, since the window stops seeing their release.
//
// Runs on the UI thread, which owns the window; it must be removed from the
// window before either is destroyed.
class DesktopInputCapture final : public xe::ui::WindowListener, public xe::ui::WindowInputListener
{
  public:
    DesktopInputCapture(xe::ui::Window& window, DesktopInputState& state) noexcept;
    DesktopInputCapture(const DesktopInputCapture&) = delete;
    DesktopInputCapture& operator=(const DesktopInputCapture&) = delete;
    DesktopInputCapture(DesktopInputCapture&&) = delete;
    DesktopInputCapture& operator=(DesktopInputCapture&&) = delete;
    ~DesktopInputCapture() override;

    void OnKeyDown(xe::ui::KeyEvent& event) override;
    void OnKeyUp(xe::ui::KeyEvent& event) override;
    void OnMouseDown(xe::ui::MouseEvent& event) override;
    void OnMouseUp(xe::ui::MouseEvent& event) override;
    void OnMouseMove(xe::ui::MouseEvent& event) override;
    void OnLostFocus(xe::ui::UISetupEvent& event) override;

  private:
    void Capture() noexcept;
    void Release() noexcept;
    // Warps the pointer back to the centre of the surface once it has drifted
    // far from it, and shifts the last position by the same displacement so
    // the warp itself reports no travel.
    void Recentre() noexcept;

    GtkWidget* surface_ = nullptr;
    DesktopInputState* state_;
    bool captured_ = false;
    bool have_last_ = false;
    std::int32_t last_x_ = 0;
    std::int32_t last_y_ = 0;
};

} // namespace x360port

#endif
