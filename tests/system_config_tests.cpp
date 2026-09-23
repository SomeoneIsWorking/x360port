#include "guarded_main.hpp"
#include "x360port/system_session.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace
{

using namespace x360port;

void Require(bool condition, std::string_view message)
{
    if (!condition)
    {
        std::cerr << "system_config_tests: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

XamPadSnapshot NoPad(XamInputRequest, void*) noexcept { return {}; }

XamPadCapabilities NoCapabilities(XamInputRequest, void*) noexcept { return {}; }

ExecutionResult Passthrough(GuestCallContext& call, GuestAddress address,
                            std::span<const std::uint64_t> arguments, void*) noexcept
{
    return call.CallOriginalBody(address, arguments);
}

// Every field valid; each case below breaks exactly one, so a refusal proves
// that field is checked rather than that the whole config was never read.
SystemSessionConfig ValidConfig(const std::filesystem::path& existing_path)
{
    SystemSessionConfig config;
    config.application_name = "x360port-system-config-tests";
    config.title_path = existing_path;
    config.storage_root = std::filesystem::temp_directory_path() / "x360port-system-config-tests";
    config.expected_title_id = 0x4D5307D5U;
    config.audio = SystemAudio::Silent;
    config.player_gamertag = "Player";
    config.input = SystemInputSource{NoPad, NoCapabilities, nullptr};
    config.overrides.push_back(SystemOverride{0x82000000U, Passthrough, nullptr});
    return config;
}

void RequireRefused(SystemSessionConfig config, RuntimeError expected, std::string_view field)
{
    const SystemSessionCreateResult created = SystemSession::CreateOffscreen(std::move(config));
    Require(!created, std::string(field) + ": an invalid config composed a console");
    Require(created.failure.error == expected,
            std::string(field) + ": refused with the wrong error: " + created.failure.detail);
    Require(!created.failure.detail.empty(), std::string(field) + ": refusal carried no detail");
}

[[nodiscard]] int RunTests(const char* program_path)
{
    const std::filesystem::path existing = std::filesystem::absolute(program_path);
    Require(std::filesystem::exists(existing), "the test binary's own path does not exist");

    SystemSessionConfig config = ValidConfig(existing);
    config.expected_title_id = 0;
    RequireRefused(std::move(config), RuntimeError::ModuleValidationFailed, "title id");

    config = ValidConfig(existing);
    config.title_path = existing.parent_path() / "no-such-title.iso";
    RequireRefused(std::move(config), RuntimeError::ModuleValidationFailed, "title path");

    config = ValidConfig(existing);
    config.storage_root = "relative/storage";
    RequireRefused(std::move(config), RuntimeError::ModuleValidationFailed, "storage root");

    config = ValidConfig(existing);
    config.input.state = nullptr;
    RequireRefused(std::move(config), RuntimeError::ImportValidationFailed, "pad reader");

    config = ValidConfig(existing);
    config.input.capabilities = nullptr;
    RequireRefused(std::move(config), RuntimeError::ImportValidationFailed, "capabilities reader");

    config = ValidConfig(existing);
    config.overrides.push_back(SystemOverride{0x82000010U, nullptr, nullptr});
    RequireRefused(std::move(config), RuntimeError::OverrideInvalid, "override handler");

    config = ValidConfig(existing);
    config.display_refresh_hz = 0;
    RequireRefused(std::move(config), RuntimeError::BackendInitializationFailed,
                   "zero display refresh");

    config = ValidConfig(existing);
    config.display_refresh_hz = kMaxDisplayRefreshHz + 1;
    RequireRefused(std::move(config), RuntimeError::BackendInitializationFailed,
                   "display refresh above the maximum");

    config = ValidConfig(existing);
    config.max_presents_per_second = config.display_refresh_hz + 1;
    RequireRefused(std::move(config), RuntimeError::BackendInitializationFailed,
                   "present limit above the display refresh");

    config = ValidConfig(existing);
    config.player_gamertag = "1Player";
    RequireRefused(std::move(config), RuntimeError::ModuleValidationFailed,
                   "gamertag starting with a digit");

    config = ValidConfig(existing);
    config.player_gamertag = "SixteenLettersXY";
    RequireRefused(std::move(config), RuntimeError::ModuleValidationFailed,
                   "gamertag longer than the console allows");

    std::cout << "system_config_tests: 11 invalid configs refused before composing a console\n";
    return EXIT_SUCCESS;
}

} // namespace

int main(int, char** argv)
{
    return x360port::tests::GuardedMain("system_config_tests",
                                        [argv] { return RunTests(argv[0]); });
}
