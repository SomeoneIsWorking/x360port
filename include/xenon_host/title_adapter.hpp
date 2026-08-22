#ifndef XENON_HOST_TITLE_ADAPTER_HPP
#define XENON_HOST_TITLE_ADAPTER_HPP

#include "xenon_host/guest_module.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace xenon_host
{

enum class Capability : std::uint8_t
{
    GuestMemory = 1U << 0U,
    KernelServices = 1U << 1U,
    Graphics = 1U << 2U,
    Audio = 1U << 3U,
    Storage = 1U << 4U,
    Networking = 1U << 5U,
};

class CapabilitySet
{
  public:
    constexpr CapabilitySet() noexcept = default;
    constexpr CapabilitySet(Capability capability) noexcept
        : bits_(static_cast<std::uint32_t>(capability))
    {
    }

    [[nodiscard]] static constexpr CapabilitySet FromBits(std::uint32_t bits) noexcept
    {
        return CapabilitySet(bits);
    }

    [[nodiscard]] constexpr std::uint32_t Bits() const noexcept { return bits_; }

    [[nodiscard]] constexpr bool Contains(CapabilitySet required) const noexcept
    {
        return (bits_ & required.bits_) == required.bits_;
    }

    friend constexpr CapabilitySet operator|(CapabilitySet left, CapabilitySet right) noexcept
    {
        return CapabilitySet::FromBits(left.bits_ | right.bits_);
    }

  private:
    explicit constexpr CapabilitySet(std::uint32_t bits) noexcept : bits_(bits) {}

    std::uint32_t bits_ = 0;
};

[[nodiscard]] constexpr CapabilitySet operator|(Capability left, Capability right) noexcept
{
    return CapabilitySet(left) | CapabilitySet(right);
}

using ImportHandler = void (*)(void* call_context) noexcept;

struct ImportBinding
{
    std::string_view library;
    std::uint32_t ordinal = 0;
    ImportHandler handler = nullptr;
};

class ValidatedGuestModule
{
  public:
    [[nodiscard]] const GuestModule& Module() const noexcept { return *module_; }

  private:
    explicit ValidatedGuestModule(const GuestModule& module) noexcept : module_(&module) {}

    const GuestModule* module_;

    friend class Host;
};

struct AdapterRunResult
{
    bool entered_guest = false;
    int exit_code = 0;
    std::string refusal;
};

class TitleAdapter
{
  public:
    virtual ~TitleAdapter() = default;

    [[nodiscard]] virtual std::string_view TitleKey() const noexcept = 0;
    [[nodiscard]] virtual std::string_view RevisionKey() const noexcept = 0;
    [[nodiscard]] virtual const GuestModule& Module() const noexcept = 0;
    [[nodiscard]] virtual CapabilitySet Capabilities() const noexcept = 0;
    [[nodiscard]] virtual std::span<const ImportBinding> ImportBindings() const noexcept = 0;

    // The adapter owns title context construction and the concrete PPC ABI.
    // Host calls this only after every shared contract has passed.
    [[nodiscard]] virtual AdapterRunResult Enter(ValidatedGuestModule module) noexcept = 0;
};

} // namespace xenon_host

#endif
