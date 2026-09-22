#ifndef X360PORT_SYSTEM_SESSION_HPP
#define X360PORT_SYSTEM_SESSION_HPP

#include "x360port/guest_call.hpp"
#include "x360port/runtime_failure.hpp"
#include "x360port/xam_input.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace x360port
{

// Where a session's sound goes. A silent session still runs the console's
// audio hardware model, because titles block on their own render callbacks;
// only the host output device is replaced by one whose cursors still advance.
enum class SystemAudio : std::uint8_t
{
    Device,
    Silent,
};

// A title-owned native implementation bound to an exact guest entry.
struct SystemOverride
{
    GuestAddress address = 0;
    NativeOverrideHandler handler = nullptr;
    void* context = nullptr;
};

// The title's controller arbitration, read by the console's input exports.
// Both readers must be non-null and thread-safe: any guest thread may poll.
struct SystemInputSource
{
    XamPadReader state = nullptr;
    XamCapabilitiesReader capabilities = nullptr;
    void* context = nullptr;
};

// Host controllers the console reads when the title's own source reports no
// pad. A measured or scripted run selects None so a controller plugged into the
// machine cannot change what the run observes.
enum class SystemHostInput : std::uint8_t
{
    None,
    Gamepads,
};

struct SystemSessionConfig
{
    // Shown as the window title and used to name the session's log.
    std::string application_name;
    // A disc image, extracted disc tree, or XEX the title adapter has already
    // authenticated. The session never searches for a substitute.
    std::filesystem::path title_path;
    // Per-user writable root for saves, profiles, and host caches. The title's
    // configuration owner resolves it from the platform's user-data location.
    std::filesystem::path storage_root;
    // The title ID the loaded module must report before its first instruction
    // runs. Zero is refused: a session always states which title it expects.
    std::uint32_t expected_title_id = 0;
    SystemAudio audio = SystemAudio::Device;
    // Asked first; while it reports no pad, the host input below answers.
    SystemInputSource input;
    SystemHostInput host_input = SystemHostInput::None;
    // Installed after the module is mapped and before its main thread resumes.
    std::vector<SystemOverride> overrides;
};

// One captured guest output image, 8-bit RGBX rows of `stride` bytes.
struct SystemFrameImage
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::size_t stride = 0;
    std::vector<std::byte> pixels;
};

struct SystemSessionCreateResult;

// The complete console: Xenia's memory, dynarec processor, kernel and XAM
// services, file system, audio, GPU, and input, composed around one title and
// its native overrides. This is the gameplay product; RuntimeContext remains
// the isolated leaf harness overrides are qualified in.
//
// One session may exist per process, because Xenia's guest address space is a
// process-wide fixed mapping.
class SystemSession final
{
  public:
    SystemSession(const SystemSession&) = delete;
    SystemSession& operator=(const SystemSession&) = delete;
    SystemSession(SystemSession&&) = delete;
    SystemSession& operator=(SystemSession&&) = delete;
    ~SystemSession();

    // An offscreen session: no window, guest output kept for capture. For
    // maintainer tools that drive and measure the product without a desktop.
    [[nodiscard]] static SystemSessionCreateResult CreateOffscreen(SystemSessionConfig config);

    // Mounts and loads the title, checks its identity, installs every
    // override, and resumes its main thread. A title that fails the identity
    // check or refuses an override never executes a guest instruction.
    [[nodiscard]] RuntimeFailure Launch();

    // Guest presents observed since launch. Monotonic.
    [[nodiscard]] std::uint64_t PresentedFrameCount() const noexcept;

    // The most recent guest output image. Fails, rather than returning an
    // empty image, before the title has presented.
    [[nodiscard]] RuntimeFailure CaptureGuestOutput(SystemFrameImage& image) const;

    [[nodiscard]] std::uint64_t NativeOverrideCalls() const noexcept;

    // Ends the process. Xenia cannot tear down a title whose guest threads are
    // running, so once Launch has succeeded this is the only way a session
    // ends; destroying a launched session ends the process with a failure
    // status and says so in the log.
    [[noreturn]] static void EndProcess(int status) noexcept;

  private:
    class Impl;
    friend RuntimeFailure RunWindowedSystem(SystemSessionConfig config);

    explicit SystemSession(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> impl_;
};

struct SystemSessionCreateResult
{
    std::unique_ptr<SystemSession> session;
    RuntimeFailure failure;

    [[nodiscard]] explicit operator bool() const noexcept { return session != nullptr && !failure; }
};

// Runs the title in a game window on the calling thread, which must be the
// process's main thread. Returns only when the session could not start; once
// the title runs, the process ends when the player closes the window or the
// title exits.
[[nodiscard]] RuntimeFailure RunWindowedSystem(SystemSessionConfig config);

} // namespace x360port

#endif
