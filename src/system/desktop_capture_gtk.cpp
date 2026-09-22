#include "desktop_capture_gtk.hpp"

#include <gtk/gtk.h>

#include <cstdlib>
#include <optional>

#include "xenia/base/logging.h"
#include "xenia/ui/window_gtk.h"

namespace x360port
{
namespace
{

// The pointer is recentred once it is this fraction of the surface away from
// the centre: far enough that ordinary motion needs no warp, near enough that
// a fast flick stays inside the window.
constexpr int kRecentreDivisor = 4;

// Xenia's GTK window delivers mouse events from its drawing area, which it
// does not expose; it is the only drawing area in the window.
GtkWidget* FindDrawingArea(GtkWidget* widget) noexcept
{
    if (GTK_IS_DRAWING_AREA(widget))
    {
        return widget;
    }
    if (!GTK_IS_CONTAINER(widget))
    {
        return nullptr;
    }
    GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
    GtkWidget* found = nullptr;
    for (GList* child = children; child != nullptr && found == nullptr; child = child->next)
    {
        found = FindDrawingArea(GTK_WIDGET(child->data));
    }
    g_list_free(children);
    return found;
}

[[nodiscard]] std::optional<DesktopMouseButton>
DesktopButton(xe::ui::MouseEvent::Button button) noexcept
{
    switch (button)
    {
    case xe::ui::MouseEvent::Button::kLeft:
        return DesktopMouseButton::Left;
    case xe::ui::MouseEvent::Button::kRight:
        return DesktopMouseButton::Right;
    case xe::ui::MouseEvent::Button::kMiddle:
        return DesktopMouseButton::Middle;
    case xe::ui::MouseEvent::Button::kX1:
        return DesktopMouseButton::Back;
    case xe::ui::MouseEvent::Button::kX2:
        return DesktopMouseButton::Forward;
    case xe::ui::MouseEvent::Button::kNone:
        break;
    }
    return std::nullopt;
}

[[nodiscard]] bool IsVirtualKeyCode(xe::ui::VirtualKey key) noexcept
{
    return static_cast<std::uint32_t>(key) <= UINT8_MAX;
}

GdkDevice* Pointer(GtkWidget* widget) noexcept
{
    return gdk_seat_get_pointer(gdk_display_get_default_seat(gtk_widget_get_display(widget)));
}

} // namespace

DesktopInputCapture::DesktopInputCapture(xe::ui::Window& window, DesktopInputState& state) noexcept
    : surface_(FindDrawingArea(static_cast<xe::ui::GTKWindow&>(window).window())), state_(&state)
{
    if (surface_ == nullptr)
    {
        XELOGE("x360port: the game window has no drawing area; the mouse cannot be captured");
    }
}

DesktopInputCapture::~DesktopInputCapture() { Release(); }

void DesktopInputCapture::OnKeyDown(xe::ui::KeyEvent& event)
{
    if (IsVirtualKeyCode(event.virtual_key()))
    {
        state_->SetKey(static_cast<std::uint8_t>(event.virtual_key()), true);
    }
}

void DesktopInputCapture::OnKeyUp(xe::ui::KeyEvent& event)
{
    if (IsVirtualKeyCode(event.virtual_key()))
    {
        state_->SetKey(static_cast<std::uint8_t>(event.virtual_key()), false);
    }
}

void DesktopInputCapture::OnMouseDown(xe::ui::MouseEvent& event)
{
    if (!captured_)
    {
        Capture();
        event.set_handled(true);
        return;
    }
    if (std::optional<DesktopMouseButton> button = DesktopButton(event.button()))
    {
        state_->SetMouseButton(*button, true);
    }
}

void DesktopInputCapture::OnMouseUp(xe::ui::MouseEvent& event)
{
    if (std::optional<DesktopMouseButton> button = DesktopButton(event.button()))
    {
        state_->SetMouseButton(*button, false);
    }
}

void DesktopInputCapture::OnMouseMove(xe::ui::MouseEvent& event)
{
    if (!captured_)
    {
        return;
    }
    if (have_last_)
    {
        state_->AddPointerTravel(event.x() - last_x_, event.y() - last_y_);
    }
    last_x_ = event.x();
    last_y_ = event.y();
    have_last_ = true;
    Recentre();
}

void DesktopInputCapture::OnLostFocus(xe::ui::UISetupEvent&)
{
    Release();
    state_->ReleaseAll();
}

void DesktopInputCapture::Capture() noexcept
{
    if (surface_ == nullptr)
    {
        return;
    }
    GdkWindow* surface_window = gtk_widget_get_window(surface_);
    GdkDisplay* display = gtk_widget_get_display(surface_);
    GdkCursor* blank = gdk_cursor_new_for_display(display, GDK_BLANK_CURSOR);
    // Pointer only, so the desktop keeps its keyboard shortcuts. Without owner
    // events every pointer event arrives relative to the surface, even outside
    // it, so successive positions always share one frame.
    GdkGrabStatus status =
        gdk_seat_grab(gdk_display_get_default_seat(display), surface_window,
                      GDK_SEAT_CAPABILITY_ALL_POINTING, FALSE, blank, nullptr, nullptr, nullptr);
    g_object_unref(blank);
    if (status != GDK_GRAB_SUCCESS)
    {
        XELOGW("x360port: the desktop refused the pointer grab (status {})",
               static_cast<int>(status));
        return;
    }
    captured_ = true;
    have_last_ = false;
    state_->SetPointerCaptured(true);
}

void DesktopInputCapture::Release() noexcept
{
    if (!captured_)
    {
        return;
    }
    gdk_seat_ungrab(gdk_display_get_default_seat(gtk_widget_get_display(surface_)));
    captured_ = false;
    have_last_ = false;
    state_->SetPointerCaptured(false);
}

void DesktopInputCapture::Recentre() noexcept
{
    GdkWindow* surface_window = gtk_widget_get_window(surface_);
    int width = gdk_window_get_width(surface_window);
    int height = gdk_window_get_height(surface_window);
    int centre_x = width / 2;
    int centre_y = height / 2;
    if (std::abs(last_x_ - centre_x) < width / kRecentreDivisor &&
        std::abs(last_y_ - centre_y) < height / kRecentreDivisor)
    {
        return;
    }
    int origin_x = 0;
    int origin_y = 0;
    gdk_window_get_origin(surface_window, &origin_x, &origin_y);
    gdk_device_warp(Pointer(surface_), gtk_widget_get_screen(surface_), origin_x + centre_x,
                    origin_y + centre_y);
    last_x_ = centre_x;
    last_y_ = centre_y;
}

} // namespace x360port
