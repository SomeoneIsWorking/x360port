#ifndef XENON_HOST_MODULE_VALIDATION_HPP
#define XENON_HOST_MODULE_VALIDATION_HPP

#include "xenon_host/host.hpp"

namespace xenon_host
{

[[nodiscard]] RunResult ValidateModule(const GuestModule& module) noexcept;
[[nodiscard]] RunResult ValidateImports(const GuestModule& module,
                                        std::span<const ImportBinding> bindings) noexcept;
[[nodiscard]] bool IsPortableKey(std::string_view key) noexcept;
[[nodiscard]] bool IsValidCapabilitySet(CapabilitySet capabilities) noexcept;

} // namespace xenon_host

#endif
