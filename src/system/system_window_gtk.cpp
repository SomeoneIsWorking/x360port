#include "system_session_impl.hpp"

#include "desktop_capture_gtk.hpp"

#include <gtk/gtk.h>

#include <atomic>
#include <cstdlib>
#include <memory>
#include <optional>
#include <thread>
#include <utility>

#include "xenia/base/logging.h"
#include "xenia/emulator.h"
#include "xenia/gpu/graphics_system.h"
#include "xenia/ui/window.h"
#include "xenia/ui/window_listener.h"
#include "xenia/ui/windowed_app_context_gtk.h"

namespace x360port
{
namespace
{

constexpr std::uint32_t kInitialWidth = 1280;
constexpr std::uint32_t kInitialHeight = 720;
// Keyboard focus priority for the window's own shortcuts, which come before the
// title's desktop input so F11 and Alt+Enter never reach the game.
constexpr std::size_t kShortcutZOrder = 0;
constexpr std::size_t kDesktopInputZOrder = 1;

// Closing the window is the player quitting the game.
class QuitOnClose final : public xe::ui::WindowListener
{
  public:
    explicit QuitOnClose(xe::ui::WindowedAppContext& app_context) noexcept
        : app_context_(&app_context)
    {
    }

    void OnClosing(xe::ui::UIEvent&) override { app_context_->QuitFromUIThread(); }

  private:
    xe::ui::WindowedAppContext* app_context_;
};

// The PC conventions for a game window: F11 or Alt+Enter toggles fullscreen.
class FullscreenShortcut final : public xe::ui::WindowInputListener
{
  public:
    explicit FullscreenShortcut(xe::ui::Window& window) noexcept : window_(&window) {}

    void OnKeyDown(xe::ui::KeyEvent& event) override
    {
        const bool alt_enter =
            event.virtual_key() == xe::ui::VirtualKey::kReturn && event.is_alt_pressed();
        if (event.virtual_key() == xe::ui::VirtualKey::kF11 || alt_enter)
        {
            window_->SetFullscreen(!window_->IsFullscreen());
            event.set_handled(true);
        }
    }

  private:
    xe::ui::Window* window_;
};

} // namespace

RuntimeFailure RunWindowedSystem(SystemSessionConfig config, SystemSessionLaunched on_launched)
{
    // Xenia presents through an Xlib/XCB Vulkan surface, so a Wayland desktop
    // must give this window to Xwayland.
    gdk_set_allowed_backends("x11");
    if (gtk_init_check(nullptr, nullptr) == FALSE)
    {
        return RuntimeFailure{RuntimeError::BackendInitializationFailed,
                              "GTK could not open a display for the game window"};
    }

    xe::ui::GTKWindowedAppContext app_context;
    std::unique_ptr<xe::ui::Window> window =
        xe::ui::Window::Create(app_context, config.application_name, kInitialWidth, kInitialHeight);
    if (window == nullptr)
    {
        return RuntimeFailure{RuntimeError::BackendInitializationFailed,
                              "the platform refused to create the game window"};
    }
    QuitOnClose quit_on_close(app_context);
    FullscreenShortcut fullscreen(*window);
    window->AddListener(&quit_on_close);
    window->AddInputListener(&fullscreen, kShortcutZOrder);
    std::optional<DesktopInputCapture> desktop_input;
    if (config.desktop_input != nullptr)
    {
        desktop_input.emplace(*window, *config.desktop_input);
        window->AddListener(&*desktop_input);
        window->AddInputListener(&*desktop_input, kDesktopInputZOrder);
    }
    if (config.start_fullscreen)
    {
        window->SetFullscreen(true);
    }
    if (!window->Open())
    {
        return RuntimeFailure{RuntimeError::BackendInitializationFailed,
                              "the platform refused to open the game window"};
    }

    // Owned through the public type so `on_launched` can be handed the session.
    SystemSession owner(std::make_unique<SystemSession::Impl>(std::move(config)));
    RuntimeFailure failure;
    std::atomic<bool> launched{false};
    // Composes and launches the console on its own thread, as Xenia requires:
    // the UI thread must keep pumping while the GPU and audio systems come up.
    // A failure is recorded for the caller and ends the UI loop.
    std::thread emulator_thread(
        [&owner, &session = *owner.impl_, &window = *window, &app_context, &failure, &launched,
         &on_launched]()
        {
            failure = session.Initialize(&window);
            if (!failure)
            {
                app_context.CallInUIThreadSynchronous(
                    [&session, &window]()
                    { window.SetPresenter(session.Emulator().graphics_system()->presenter()); });
                failure = session.Launch();
            }
            if (failure)
            {
                app_context.CallInUIThread([&app_context]() { app_context.QuitFromUIThread(); });
                return;
            }
            launched.store(true, std::memory_order_release);
            if (on_launched)
            {
                on_launched(owner);
            }
            session.Emulator().WaitUntilExit();
            app_context.CallInUIThread([&app_context]() { app_context.QuitFromUIThread(); });
        });
    app_context.RunMainGTKLoop();

    if (!launched.load(std::memory_order_acquire))
    {
        // The UI loop only ends before launch when the emulator thread ended
        // it, so the thread has finished and nothing guest-side ran.
        emulator_thread.join();
        window->SetPresenter(nullptr);
        if (desktop_input)
        {
            window->RemoveInputListener(&*desktop_input);
            window->RemoveListener(&*desktop_input);
        }
        return failure;
    }
    // The player closed the window or the title exited. Guest threads are
    // still live and Xenia cannot stop them, so the process ends here; the
    // emulator thread is still waiting on them and is never joined.
    emulator_thread.detach();
    SystemSession::EndProcess(EXIT_SUCCESS);
}

} // namespace x360port
