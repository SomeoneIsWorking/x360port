#include "xenon_host/host.hpp"

#include "module_validation.hpp"

#include <array>

namespace xenon_host
{

RunResult Host::Run(TitleAdapter& adapter, RunRequest request) noexcept
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
    if (RunResult result = ValidateModule(module); !result)
    {
        return result;
    }
    if (RunResult result = ValidateImports(module, adapter.ImportBindings()); !result)
    {
        return result;
    }

    AdapterRunResult adapter_result = adapter.Enter(ValidatedGuestModule(module));
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
