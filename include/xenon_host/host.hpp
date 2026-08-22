#ifndef XENON_HOST_HOST_HPP
#define XENON_HOST_HOST_HPP

#include "xenon_host/title_adapter.hpp"

#include <cstdint>
#include <string>

namespace xenon_host
{

enum class RunError : std::uint8_t
{
    None,
    InvalidTitleKey,
    InvalidRevisionKey,
    InvalidCapabilitySet,
    MissingCapability,
    InvalidImageDigest,
    ImageSizeMismatch,
    ImageAddressOverflow,
    GuestMemoryReservationFailed,
    InvalidGuestMemoryAlignment,
    GuestMemoryCommitFailed,
    ImageDigestMismatch,
    InvalidCodeRange,
    EntryPointOutsideCode,
    FunctionCountMismatch,
    EmptyFunctionMap,
    InvalidFunctionAddress,
    NullFunctionThunk,
    UnsortedFunctionMap,
    EntryPointMissing,
    FunctionMapDigestMismatch,
    ImportCountMismatch,
    InvalidImport,
    UnsortedImportManifest,
    ImportManifestDigestMismatch,
    ImportBindingCountMismatch,
    ImportBindingMismatch,
    NullImportHandler,
    AdapterRefused,
    Count,
};

struct RunRequest
{
    CapabilitySet required_capabilities = Capability::GuestMemory;
};

struct RunResult
{
    RunError error = RunError::None;
    int guest_exit_code = 0;
    std::string detail;

    [[nodiscard]] explicit operator bool() const noexcept { return error == RunError::None; }
};

class Host
{
  public:
    [[nodiscard]] static RunResult Run(TitleAdapter& adapter, RunRequest request = {}) noexcept;
};

[[nodiscard]] std::string_view ToString(RunError error) noexcept;

} // namespace xenon_host

#endif
