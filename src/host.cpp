#include "xenon_host/host.hpp"

#include "host_run.hpp"
#include "module_validation.hpp"

#include <array>

namespace xenon_host
{

RunResult Host::Run(TitleAdapter& adapter, RunRequest request) noexcept
{
    return HostRunner::Run(adapter, request, GuestMemoryLoader::PlatformOperations());
}

RunResult HostRunner::Run(TitleAdapter& adapter, RunRequest request,
                          const GuestVirtualMemoryOps& operations) noexcept
{
    if (!IsPortableKey(adapter.TitleKey()))
    {
        return {.error = RunError::InvalidTitleKey, .detail = "title key is not portable"};
    }
    if (!IsPortableKey(adapter.RevisionKey()))
    {
        return {.error = RunError::InvalidRevisionKey, .detail = "revision key is not portable"};
    }
    if (!IsValidCapabilitySet(adapter.Capabilities()) ||
        !IsValidCapabilitySet(request.required_capabilities))
    {
        return {.error = RunError::InvalidCapabilitySet,
                .detail = "capability set contains undefined bits"};
    }
    if (!adapter.Capabilities().Contains(request.required_capabilities))
    {
        return {.error = RunError::MissingCapability,
                .detail = "title adapter does not provide every required capability"};
    }

    const GuestModule& module = adapter.Module();
    GuestMemoryLoadResult loaded =
        GuestMemoryLoader::Load(module.Descriptor().image, module.ImageBytes(), operations);
    if (!loaded)
    {
        switch (loaded.error)
        {
        case GuestMemoryLoadError::InvalidImageDigest:
            return {.error = RunError::InvalidImageDigest, .detail = std::string(loaded.detail)};
        case GuestMemoryLoadError::ImageSizeMismatch:
            return {.error = RunError::ImageSizeMismatch, .detail = std::string(loaded.detail)};
        case GuestMemoryLoadError::ImageAddressOverflow:
            return {.error = RunError::ImageAddressOverflow, .detail = std::string(loaded.detail)};
        case GuestMemoryLoadError::ReservationFailed:
            return {.error = RunError::GuestMemoryReservationFailed,
                    .detail = std::string(loaded.detail)};
        case GuestMemoryLoadError::InvalidWindowAlignment:
            return {.error = RunError::InvalidGuestMemoryAlignment,
                    .detail = std::string(loaded.detail)};
        case GuestMemoryLoadError::CommitFailed:
            return {.error = RunError::GuestMemoryCommitFailed,
                    .detail = std::string(loaded.detail)};
        case GuestMemoryLoadError::ImageDigestMismatch:
            return {.error = RunError::ImageDigestMismatch, .detail = std::string(loaded.detail)};
        case GuestMemoryLoadError::None:
            break;
        }
        return {.error = RunError::GuestMemoryReservationFailed,
                .detail = "guest memory load failed without a named cause"};
    }
    GuestMemory& memory = loaded.memory;
    if (RunResult result = ValidateModule(module, memory); !result)
    {
        return result;
    }
    if (RunResult result = ValidateImports(module, adapter.ImportBindings()); !result)
    {
        return result;
    }

    AdapterRunResult adapter_result = adapter.Enter(ValidatedGuestModule(module, memory));
    if (!adapter_result.entered_guest)
    {
        return {.error = RunError::AdapterRefused,
                .detail = adapter_result.refusal.empty() ? "title adapter refused guest entry"
                                                         : std::move(adapter_result.refusal)};
    }
    return {
        .error = RunError::None,
        .guest_exit_code = adapter_result.exit_code,
        .detail = {},
    };
}

std::string_view ToString(RunError error) noexcept
{
    constexpr std::array Names = {
        "none",
        "invalid-title-key",
        "invalid-revision-key",
        "invalid-capability-set",
        "missing-capability",
        "invalid-image-digest",
        "image-size-mismatch",
        "image-address-overflow",
        "guest-memory-reservation-failed",
        "invalid-guest-memory-alignment",
        "guest-memory-commit-failed",
        "image-digest-mismatch",
        "invalid-code-range",
        "entry-point-outside-code",
        "function-count-mismatch",
        "empty-function-map",
        "invalid-function-address",
        "null-function-thunk",
        "unsorted-function-map",
        "entry-point-missing",
        "function-map-digest-mismatch",
        "import-count-mismatch",
        "invalid-import",
        "unsorted-import-manifest",
        "import-manifest-digest-mismatch",
        "import-binding-count-mismatch",
        "import-binding-mismatch",
        "null-import-handler",
        "adapter-refused",
    };
    const std::size_t index = static_cast<std::size_t>(error);
    return index < Names.size() ? Names[index] : "unknown";
}

} // namespace xenon_host
