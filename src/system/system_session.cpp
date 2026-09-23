#include "system_session_impl.hpp"

#include "system_input_driver.hpp"

#include <SDL.h>

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "xenia/apu/sdl/sdl_audio_system.h"
#include "xenia/base/cvar.h"
#include "xenia/base/logging.h"
#include "xenia/cpu/cpu_flags.h"
#include "xenia/cpu/processor.h"
#include "xenia/cpu/xex_module.h"
#include "xenia/emulator.h"
#include "xenia/gpu/command_processor.h"
#include "xenia/gpu/gpu_flags.h"
#include "xenia/gpu/graphics_system.h"
#include "xenia/gpu/vulkan/vulkan_graphics_system.h"
#include "xenia/hid/input_driver.h"
#include "xenia/hid/sdl/sdl_hid.h"
#include "xenia/kernel/kernel_state.h"
#include "xenia/kernel/user_module.h"
#include "xenia/kernel/xam/profile_manager.h"
#include "xenia/kernel/xam/xam_state.h"
#include "xenia/kernel/xthread.h"
#include "xenia/ui/presenter.h"

DECLARE_path(log_file);

namespace x360port
{
namespace
{

// Xenia's status macros expand to casts through this unqualified name.
using xe::X_STATUS;

[[nodiscard]] RuntimeFailure Failure(RuntimeError error, std::string detail)
{
    return RuntimeFailure{error, std::move(detail)};
}

[[nodiscard]] std::string HexWord(std::uint32_t value)
{
    constexpr std::string_view kDigits = "0123456789ABCDEF";
    std::string text(8, '0');
    for (std::size_t index = 0; index < text.size(); ++index)
    {
        text[text.size() - index - 1] = kDigits[(value >> (index * 4U)) & 0xFU];
    }
    return text;
}

// A failed override on a free-running guest thread leaves that thread's state
// unknown, and no host call is waiting to receive the failure. Ending the
// process with the reason is the only answer that does not continue in an
// invalid state.
[[noreturn]] void EndTitleAfterOverrideFailure(const RuntimeFailure& failure) noexcept
{
    xe::FatalError("x360port: a native override failed on a guest thread: " + failure.detail);
}

} // namespace

XeniaLoggingScope::XeniaLoggingScope(const std::string& application_name,
                                     const std::filesystem::path& log_path)
{
    cvars::log_file = log_path;
    xe::InitializeLogging(application_name);
}

XeniaLoggingScope::~XeniaLoggingScope() { xe::ShutdownLogging(); }

SystemSession::Impl::Impl(SystemSessionConfig config) noexcept : config_(std::move(config)) {}

SystemSession::Impl::~Impl()
{
    if (launched_)
    {
        XELOGE("x360port: a launched system session was destroyed; Xenia cannot stop a running "
               "title, so the process ends here");
        SystemSession::EndProcess(EXIT_FAILURE);
    }
    // Nothing has executed, so Xenia's ordinary teardown applies. The
    // emulator goes before the override table its trampolines would read.
    emulator_.reset();
}

RuntimeFailure SystemSession::Impl::ValidateConfig() const
{
    if (config_.expected_title_id == 0)
    {
        return Failure(RuntimeError::ModuleValidationFailed,
                       "a system session must state the title ID it expects to launch");
    }
    std::error_code error;
    if (!std::filesystem::exists(config_.title_path, error))
    {
        return Failure(RuntimeError::ModuleValidationFailed,
                       "the title path does not exist: " + config_.title_path.string());
    }
    if (config_.storage_root.empty() || !config_.storage_root.is_absolute())
    {
        return Failure(RuntimeError::ModuleValidationFailed,
                       "the storage root must be an absolute per-user directory");
    }
    if (!config_.player_gamertag.empty() &&
        !xe::kernel::xam::ProfileManager::IsGamertagValid(config_.player_gamertag))
    {
        return Failure(RuntimeError::ModuleValidationFailed,
                       "the console refuses the gamertag \"" + config_.player_gamertag +
                           "\": 1-15 letters, digits and single inner spaces, starting "
                           "with a letter");
    }
    if (config_.input.state == nullptr || config_.input.capabilities == nullptr)
    {
        return Failure(RuntimeError::ImportValidationFailed,
                       "a system session requires both controller readers");
    }
    if (config_.display_refresh_hz < kMinDisplayRefreshHz ||
        config_.display_refresh_hz > kMaxDisplayRefreshHz)
    {
        return Failure(RuntimeError::BackendInitializationFailed,
                       "the display refresh must be between " +
                           std::to_string(kMinDisplayRefreshHz) + " and " +
                           std::to_string(kMaxDisplayRefreshHz) + " Hz, not " +
                           std::to_string(config_.display_refresh_hz));
    }
    if (config_.max_presents_per_second > config_.display_refresh_hz)
    {
        return Failure(RuntimeError::BackendInitializationFailed,
                       "the present limit must not exceed the display refresh of " +
                           std::to_string(config_.display_refresh_hz) + " Hz, not " +
                           std::to_string(config_.max_presents_per_second));
    }
    for (const SystemOverride& entry : config_.overrides)
    {
        if (entry.handler == nullptr)
        {
            return Failure(RuntimeError::OverrideInvalid,
                           "native override at 0x" + HexWord(entry.address) + " has no handler");
        }
    }
    return {};
}

RuntimeFailure SystemSession::Impl::Initialize(xe::ui::Window* window)
{
    if (RuntimeFailure failure = ValidateConfig())
    {
        return failure;
    }
    if (!instance_.Acquire())
    {
        return Failure(RuntimeError::InstanceAlreadyActive,
                       "Xenia guest memory uses a process-wide fixed mapping; only one x360port "
                       "runtime or system session may be active");
    }
    std::error_code directory_error;
    std::filesystem::create_directories(config_.storage_root, directory_error);
    if (directory_error)
    {
        return Failure(RuntimeError::MemoryInitializationFailed,
                       "the storage root could not be created: " + directory_error.message());
    }
    logging_.emplace(config_.application_name,
                     config_.storage_root / "logs" / (config_.application_name + ".log"));

    // Xenia's vblank thread paces at framerate_limit while vsync is on.
    cvars::vsync = true;
    cvars::framerate_limit = config_.display_refresh_hz;
    cvars::guest_present_limit = config_.max_presents_per_second;
    cvars::perf_map = config_.write_perf_map;

    if (config_.audio == SystemAudio::Silent)
    {
        // The dummy device consumes buffers on the audio clock, so the title's
        // render callbacks keep firing exactly as they would with sound.
        SDL_SetHint(SDL_HINT_AUDIODRIVER, "dummy");
    }

    emulator_ =
        std::make_unique<xe::Emulator>("", config_.storage_root, config_.storage_root / "content",
                                       config_.storage_root / "cache_host");
    emulator_->on_launch.AddListener([this](std::uint32_t title_id, std::string_view)
                                     { OnLaunch(title_id); });

    const X_STATUS setup = emulator_->Setup(
        window, nullptr, true, window == nullptr, xe::apu::sdl::SDLAudioSystem::Create,
        []() -> std::unique_ptr<xe::gpu::GraphicsSystem>
        { return std::make_unique<xe::gpu::vulkan::VulkanGraphicsSystem>(); },
        [this](xe::ui::Window* input_window) { return CreateInputDrivers(input_window); });
    if (XFAILED(setup))
    {
        return Failure(RuntimeError::BackendInitializationFailed,
                       "Xenia refused to compose the console: status 0x" + HexWord(setup));
    }
    if (input_failure_)
    {
        return input_failure_;
    }
    if (RuntimeFailure failure = SignInLocalPlayer())
    {
        return failure;
    }
    call_context_.emplace(*emulator_->memory(), *emulator_->processor());
    overrides_.Bind(*emulator_->processor(), *call_context_, statistics_,
                    EndTitleAfterOverrideFailure);
    return {};
}

RuntimeFailure SystemSession::Impl::SignInLocalPlayer()
{
    if (config_.player_gamertag.empty())
    {
        return {};
    }
    xe::kernel::xam::ProfileManager& profiles =
        *emulator_->kernel_state()->xam_state()->profile_manager();
    // Accounts are ordered by XUID, so the same profile is chosen every time.
    if (profiles.GetAccounts()->empty() &&
        !profiles.CreateProfile(config_.player_gamertag, /*autologin=*/false))
    {
        return Failure(RuntimeError::BackendInitializationFailed,
                       "the local player's profile could not be created under the storage root");
    }
    const std::uint64_t xuid = profiles.GetAccounts()->begin()->first;
    profiles.Login(xuid, 0, /*notify=*/false);
    if (profiles.GetProfile(std::uint8_t{0}) == nullptr)
    {
        return Failure(RuntimeError::BackendInitializationFailed,
                       "the local player's profile under the storage root could not be signed in");
    }
    return {};
}

std::vector<std::unique_ptr<xe::hid::InputDriver>>
SystemSession::Impl::CreateInputDrivers(xe::ui::Window* window)
{
    // One driver answers the console: Xenia asks its drivers in order and
    // takes the first connected pad, which would silence a host gamepad
    // whenever the title's source reports one. The system driver merges them.
    std::unique_ptr<xe::hid::InputDriver> gamepads;
    if (config_.host_input == SystemHostInput::Gamepads)
    {
        gamepads = xe::hid::sdl::Create(window, kHostInputZOrder);
        const X_STATUS status = gamepads->Setup();
        if (XFAILED(status))
        {
            input_failure_ =
                Failure(RuntimeError::BackendInitializationFailed,
                        "the host gamepad driver refused to start: status 0x" + HexWord(status));
            return {};
        }
    }
    std::vector<std::unique_ptr<xe::hid::InputDriver>> drivers;
    drivers.push_back(CreateSystemInputDriver(config_.input, std::move(gamepads)));
    return drivers;
}

RuntimeFailure SystemSession::Impl::Launch()
{
    if (emulator_ == nullptr)
    {
        return Failure(RuntimeError::LoadStateInvalid,
                       "the session must be initialized before it launches");
    }
    const X_STATUS launched = emulator_->LaunchPath(config_.title_path);
    // Xenia has created guest threads from here on, even when it then refused
    // the title or this session held its main thread.
    launched_ = true;
    if (activation_failure_)
    {
        return activation_failure_;
    }
    if (XFAILED(launched))
    {
        return Failure(RuntimeError::ModuleRegistrationFailed,
                       "Xenia could not launch the title: status 0x" + HexWord(launched));
    }
    return {};
}

void SystemSession::Impl::OnLaunch(std::uint32_t title_id)
{
    activation_failure_ = ActivateTitle(title_id);
    if (activation_failure_)
    {
        HoldMainThread();
    }
}

RuntimeFailure SystemSession::Impl::ActivateTitle(std::uint32_t title_id)
{
    if (title_id != config_.expected_title_id)
    {
        return Failure(RuntimeError::ModuleValidationFailed,
                       "the loaded title reports ID 0x" + HexWord(title_id) +
                           ", not the expected 0x" + HexWord(config_.expected_title_id));
    }
    auto module = emulator_->kernel_state()->GetExecutableModule();
    const PESection* text = module ? module->xex_module()->GetPESection(".text") : nullptr;
    if (text == nullptr || text->size == 0)
    {
        return Failure(RuntimeError::ModuleValidationFailed,
                       "the loaded title has no executable .text section to bind overrides in");
    }
    const CodeRange code{text->address, text->size};
    for (const SystemOverride& entry : config_.overrides)
    {
        if (RuntimeFailure failure =
                overrides_.Install(entry.address, entry.handler, entry.context, code))
        {
            failure.detail += " (guest address 0x" + HexWord(entry.address) + ")";
            return failure;
        }
    }
    XELOGI("x360port: title 0x{:08X} verified; {} native overrides installed in .text "
           "[0x{:08X}, 0x{:08X})",
           title_id, config_.overrides.size(), text->address, text->address + text->size);
    return {};
}

void SystemSession::Impl::HoldMainThread()
{
    // The emulator resumes the main thread after this callback returns. One
    // extra suspension keeps a refused title from executing any instruction;
    // the session is then torn down without it ever running.
    auto threads =
        emulator_->kernel_state()->object_table()->GetObjectsByType<xe::kernel::XThread>();
    for (const auto& thread : threads)
    {
        if (thread->main_thread())
        {
            static_cast<void>(thread->Suspend());
        }
    }
}

std::uint64_t SystemSession::Impl::PresentedFrameCount() const noexcept
{
    xe::gpu::GraphicsSystem* graphics = emulator_ ? emulator_->graphics_system() : nullptr;
    xe::gpu::CommandProcessor* commands = graphics ? graphics->command_processor() : nullptr;
    return commands ? commands->guest_swap_count() : 0;
}

FrameIntervalHistogram SystemSession::Impl::FrameIntervals() const noexcept
{
    static_assert(xe::gpu::CommandProcessor::kSwapIntervalBucketMicroseconds ==
                          FrameIntervalHistogram::kBucketMicroseconds &&
                      xe::gpu::CommandProcessor::kSwapIntervalBucketCount ==
                          FrameIntervalHistogram::kBucketCount,
                  "the frame-interval buckets must match the command processor's");
    xe::gpu::GraphicsSystem* graphics = emulator_ ? emulator_->graphics_system() : nullptr;
    xe::gpu::CommandProcessor* commands = graphics ? graphics->command_processor() : nullptr;
    return commands ? FrameIntervalHistogram(commands->swap_interval_buckets())
                    : FrameIntervalHistogram();
}

RuntimeFailure SystemSession::Impl::CaptureGuestOutput(SystemFrameImage& image) const
{
    xe::gpu::GraphicsSystem* graphics = emulator_ ? emulator_->graphics_system() : nullptr;
    xe::ui::Presenter* presenter = graphics ? graphics->presenter() : nullptr;
    if (presenter == nullptr)
    {
        return Failure(RuntimeError::LoadStateInvalid,
                       "the session has no presenter to capture guest output from");
    }
    xe::ui::RawImage raw;
    if (!presenter->CaptureGuestOutput(raw))
    {
        return Failure(RuntimeError::ExecutionFailed, "the title has not presented a frame yet");
    }
    image.width = raw.width;
    image.height = raw.height;
    image.stride = raw.stride;
    image.pixels.resize(raw.data.size());
    std::memcpy(image.pixels.data(), raw.data.data(), raw.data.size());
    return {};
}

SystemExecutionCounts SystemSession::Impl::ExecutionCounts() const noexcept
{
    const xe::cpu::TranslationCounts translation = emulator_->processor()->translation_counts();
    return {
        .translated_functions = translation.defined_functions,
        .translation_failures = translation.failed_functions,
        .host_code_bytes = translation.host_code_bytes,
        .native_override_calls = std::atomic_ref<std::uint64_t>(statistics_.native_override_calls)
                                     .load(std::memory_order_relaxed),
    };
}

RuntimeFailure SystemSession::Impl::ReadGuestMemory(GuestAddress address,
                                                    std::span<std::byte> bytes) const
{
    if (!call_context_)
    {
        return Failure(RuntimeError::LoadStateInvalid,
                       "the session has no guest memory before it is initialized");
    }
    return call_context_->ReadMappedGuestMemory(address, bytes);
}

void SystemSession::EndProcess(int status) noexcept
{
    xe::FlushLog();
    std::quick_exit(status);
}

SystemSession::SystemSession(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}

SystemSession::~SystemSession() = default;

SystemSessionCreateResult SystemSession::CreateOffscreen(SystemSessionConfig config)
{
    auto impl = std::make_unique<Impl>(std::move(config));
    if (RuntimeFailure failure = impl->Initialize(nullptr))
    {
        return {nullptr, std::move(failure)};
    }
    return {std::unique_ptr<SystemSession>(new SystemSession(std::move(impl))), {}};
}

RuntimeFailure SystemSession::Launch() { return impl_->Launch(); }

std::uint64_t SystemSession::PresentedFrameCount() const noexcept
{
    return impl_->PresentedFrameCount();
}

FrameIntervalHistogram SystemSession::FrameIntervals() const noexcept
{
    return impl_->FrameIntervals();
}

RuntimeFailure SystemSession::CaptureGuestOutput(SystemFrameImage& image) const
{
    return impl_->CaptureGuestOutput(image);
}

RuntimeFailure SystemSession::ReadGuestMemory(GuestAddress address,
                                              std::span<std::byte> bytes) const
{
    return impl_->ReadGuestMemory(address, bytes);
}

SystemExecutionCounts SystemSession::ExecutionCounts() const noexcept
{
    return impl_->ExecutionCounts();
}

} // namespace x360port
