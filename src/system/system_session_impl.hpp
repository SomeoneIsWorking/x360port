#ifndef X360PORT_SYSTEM_SESSION_IMPL_HPP
#define X360PORT_SYSTEM_SESSION_IMPL_HPP

#include "../override_dispatch.hpp"
#include "../xenia_instance_lease.hpp"
#include "system_call_context.hpp"
#include "x360port/system_session.hpp"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace xe
{
class Emulator;
namespace hid
{
class InputDriver;
}
namespace ui
{
class Window;
}
} // namespace xe

namespace x360port
{

// Owns Xenia's logger for the session's lifetime. Xenia's logger blocks the
// first thread that logs when it was never initialised, and must be drained
// before the process exits. The log is written to `log_path`, never beside the
// executable, which may be a read-only install.
class XeniaLoggingScope final
{
  public:
    XeniaLoggingScope(const std::string& application_name, const std::filesystem::path& log_path);
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
    [[nodiscard]] FrameIntervalHistogram FrameIntervals() const noexcept;
    [[nodiscard]] RuntimeFailure CaptureGuestOutput(SystemFrameImage& image) const;
    [[nodiscard]] SystemExecutionCounts ExecutionCounts() const noexcept;
    [[nodiscard]] RuntimeFailure ReadGuestMemory(GuestAddress address,
                                                 std::span<std::byte> bytes) const;

    [[nodiscard]] xe::Emulator& Emulator() const noexcept { return *emulator_; }
    [[nodiscard]] const SystemSessionConfig& Config() const noexcept { return config_; }

  private:
    // Window listeners are ordered by z; host input sits above the window's
    // own shortcuts, as Xenia's application places it.
    static constexpr std::size_t kHostInputZOrder = 1;

    [[nodiscard]] RuntimeFailure ValidateConfig() const;
    [[nodiscard]] RuntimeFailure SignInLocalPlayer();
    [[nodiscard]] std::vector<std::unique_ptr<xe::hid::InputDriver>>
    CreateInputDrivers(xe::ui::Window* window);
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
    RuntimeFailure input_failure_;
    RuntimeFailure activation_failure_;
    bool launched_ = false;
};

} // namespace x360port

#endif
