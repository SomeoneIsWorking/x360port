#ifndef X360PORT_SYSTEM_SESSION_HPP
#define X360PORT_SYSTEM_SESSION_HPP

#include "x360port/desktop_input.hpp"
#include "x360port/frame_intervals.hpp"
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

// Host controllers merged with the title's own source, so either drives the
// game. A measured or scripted run selects None so a controller plugged into
// the machine cannot change what the run observes.
enum class SystemHostInput : std::uint8_t
{
    None,
    Gamepads,
};

// The Xbox 360's own display refresh, and the vblank rates a session accepts.
inline constexpr std::uint32_t kConsoleDisplayRefreshHz = 60;
inline constexpr std::uint32_t kMinDisplayRefreshHz = 1;
inline constexpr std::uint32_t kMaxDisplayRefreshHz = 1000;

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
    // The gamertag of the local player signed in to controller slot 0 before
    // the title starts, so it can save; the console refuses saves to a title
    // with nobody signed in. The first session under a storage root creates
    // the profile; later ones sign in the profile already there, whatever its
    // name. Empty signs nobody in. A name the console would refuse is refused.
    std::string player_gamertag;
    SystemAudio audio = SystemAudio::Device;
    // Asked first; while it reports no pad, the host input below answers.
    SystemInputSource input;
    SystemHostInput host_input = SystemHostInput::None;
    // Where the game window reports its keyboard and mouse, for the title's
    // source to read. Title-owned and alive for the whole session; null leaves
    // the desktop devices out of the game. An offscreen session has no window
    // and never writes to it.
    DesktopInputState* desktop_input = nullptr;
    // The console's vertical-blank rate, in the refusal-checked range
    // [kMinDisplayRefreshHz, kMaxDisplayRefreshHz]. A title that presents on
    // every Nth vblank presents at display_refresh_hz / N frames per second;
    // whether its game clock follows the vblank or the host clock is the
    // title's property, for its adapter to establish before raising this.
    std::uint32_t display_refresh_hz = kConsoleDisplayRefreshHz;
    // A host ceiling on guest presents per second, at most
    // display_refresh_hz; zero leaves the vblank rate as the only limit. A
    // present that arrives within 1 / max_presents_per_second of the previous
    // one waits, and a later one is never delayed, so a title may run its
    // vblank fast enough that vblank pacing never rounds a slow frame up.
    std::uint32_t max_presents_per_second = 0;
    // Appends each translated guest function to /tmp/perf-<pid>.map, the
    // symbol file Linux perf reads for JIT code. Diagnostic; Linux only.
    bool write_perf_map = false;
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

// Execution accounting for a launched session. Every guest function runs as
// Xenia-translated host code: the system session has no interpreter fallback,
// so a function Xenia cannot translate is counted in translation_failures and
// its guest call fails rather than running some other way.
struct SystemExecutionCounts
{
    std::uint64_t translated_functions = 0;
    std::uint64_t translation_failures = 0;
    std::uint64_t host_code_bytes = 0;
    std::uint64_t native_override_calls = 0;
};

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

    // Host time between consecutive guest presents since launch. Monotonic:
    // subtract an earlier snapshot to report a window.
    [[nodiscard]] FrameIntervalHistogram FrameIntervals() const noexcept;

    // The most recent guest output image. Fails, rather than returning an
    // empty image, before the title has presented.
    [[nodiscard]] RuntimeFailure CaptureGuestOutput(SystemFrameImage& image) const;

    [[nodiscard]] SystemExecutionCounts ExecutionCounts() const noexcept;

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
