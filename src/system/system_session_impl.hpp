#ifndef X360PORT_SYSTEM_SESSION_IMPL_HPP
#define X360PORT_SYSTEM_SESSION_IMPL_HPP

#include "../override_dispatch.hpp"
#include "../xenia_instance_lease.hpp"
#include "system_call_context.hpp"
#include "x360port/system_session.hpp"

#include <memory>
#include <optional>

namespace xe
{
class Emulator;
namespace ui
{
class Window;
}
} // namespace xe

namespace x360port
{

// Owns Xenia's logger for the session's lifetime. Xenia's logger blocks the
// first thread that logs when it was never initialised, and must be drained
// before the process exits.
class XeniaLoggingScope final
{
  public:
    explicit XeniaLoggingScope(const std::string& application_name);
    XeniaLoggingScope(const XeniaLoggingScope&) = delete;
    XeniaLoggingScope& operator=(const XeniaLoggingScope&) = delete;
    XeniaLoggingScope(XeniaLoggingScope&&) = delete;
    XeniaLoggingScope& operator=(XeniaLoggingScope&&) = delete;
    ~XeniaLoggingScope();
};

class SystemSession::Impl final
{
  public:
    explicit Impl(SystemSessionConfig config) noexcept;
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;
    ~Impl();

    // Composes the console. A null window selects offscreen presentation.
    [[nodiscard]] RuntimeFailure Initialize(xe::ui::Window* window);
    [[nodiscard]] RuntimeFailure Launch();

    [[nodiscard]] std::uint64_t PresentedFrameCount() const noexcept;
    [[nodiscard]] RuntimeFailure CaptureGuestOutput(SystemFrameImage& image) const;
    [[nodiscard]] std::uint64_t NativeOverrideCalls() const noexcept;

    [[nodiscard]] xe::Emulator& Emulator() const noexcept { return *emulator_; }
    [[nodiscard]] const SystemSessionConfig& Config() const noexcept { return config_; }

  private:
    [[nodiscard]] RuntimeFailure ValidateConfig() const;
    // Runs on the launching thread after the module is loaded and before its
    // main thread resumes.
    void OnLaunch(std::uint32_t title_id);
    [[nodiscard]] RuntimeFailure ActivateTitle(std::uint32_t title_id);
    void HoldMainThread();

    SystemSessionConfig config_;
    // Declared first so it is released last, after every Xenia owner below.
    XeniaInstanceLease instance_;
    std::optional<XeniaLoggingScope> logging_;
    std::unique_ptr<xe::Emulator> emulator_;
    std::optional<SystemCallContext> call_context_;
    OverrideDispatch overrides_;
    // Guest threads bump the override counter through atomic_ref; a const
    // reader must be able to form one too.
    mutable JitStatistics statistics_{};
    RuntimeFailure activation_failure_;
    bool launched_ = false;
};

} // namespace x360port

#endif
